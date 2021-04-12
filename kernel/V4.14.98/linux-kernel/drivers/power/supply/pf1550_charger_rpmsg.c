/*
 * pf1550_charger_rpmsg.c - regulator driver for the PF1550
 *
 * Copyright (C) 2018 NXP Semiconductor, Inc.
 * Xinyu Chen <xinyu.chen@nxp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/mfd/pf1550.h>
#include <linux/rpmsg.h>
#include <linux/virtio.h>
#include <linux/imx_rpmsg.h>
#include <linux/pm_qos.h>

#define RPMSG_TIMEOUT 1000
#define PF1550_CHARGER_NAME		"pf1550-charger-rpmsg"
#define PF1550_DEFAULT_CONSTANT_VOLT	4200000
#define PF1550_DEFAULT_MIN_SYSTEM_VOLT	3500000
#define PF1550_DEFAULT_THERMAL_TEMP	75

static const char *pf1550_charger_model		= "PF1550";
static const char *pf1550_charger_manufacturer	= "Freescale";

enum pf1550_rpmsg_type {
	PMIC_RPMSG_REQUEST,
	PMIC_RPMSG_REPLY,
	PMIC_RPMSG_NOTIFY,
};

enum pf1550_rpmsg_cmd {
	PF1550_ENABLE,
	PF1550_DISABLE,
	PF1550_IS_ENABLED,
	PF1550_SET_VOL,
	PF1550_GET_VOL,
	PF1550_GET_REG,
	PF1550_SET_REG,
	PF1550_SET_STBY_VOL,
	PF1550_RECV_INT,
	PF1550_BATT_VOL,
};

enum pf1550_resp {
	PF1550_SUCCESS,
	PF1550_FAILED,
	PF1550_UNSURPPORT,
};

enum pf1550_status {
	PF1550_DISABLED,
	PF1550_ENABLED,
};


struct pf1550_chg_rpmsg_pkg {
	/* common head */
	struct imx_rpmsg_head header;
	u8 reg; /* pf1550 register id */
	u8 response;
	u8 status; /* CHG_INT val in notification */
	u32 val; /* register r/w value, and interrupt status */
} __attribute__ ((packed));

struct pf1550_chg_rpmsg {
	struct rpmsg_device *rpdev;
	struct device *dev;
	struct pf1550_chg_rpmsg_pkg *msg;
	struct completion cmd_complete;
	struct pm_qos_request pm_qos_req;
	struct mutex lock;
	struct pf1550_charger *charger;
};

static struct pf1550_chg_rpmsg chg_rpmsg;

struct pf1550_charger {
	struct device *dev;
	struct power_supply *charger;
	struct power_supply_desc psy_desc;

	u32 constant_volt;
	u32 min_system_volt;
	u32 thermal_regulation_temp;
        struct pf1550_chg_rpmsg *rpmsg_info;
};

static void pf1550_charger_irq_handler(struct pf1550_charger *chg,
		unsigned char status, unsigned int value);

static int pf1550_send_message(struct pf1550_chg_rpmsg_pkg *msg,
			       struct pf1550_chg_rpmsg *info)
{
	int err;

	if (!info->rpdev) {
		dev_dbg(info->dev,
			 "rpmsg channel not ready, m4 image ready?\n");
		return -EINVAL;
	}

	mutex_lock(&info->lock);
	pm_qos_add_request(&info->pm_qos_req, PM_QOS_CPU_DMA_LATENCY, 0);

	msg->header.cate = IMX_RPMSG_PMIC;
	msg->header.major = IMX_RMPSG_MAJOR;
	msg->header.minor = IMX_RMPSG_MINOR;
	msg->header.type = 0;

	dev_dbg(&info->rpdev->dev, "sent cmd:%u, reg:0x%x, val:0x%x.\n",
		  msg->header.cmd, msg->reg, msg->val);

	/* wait response from rpmsg */
	reinit_completion(&info->cmd_complete);

	err = rpmsg_send(info->rpdev->ept, (void *)msg,
			    sizeof(struct pf1550_chg_rpmsg_pkg));
	if (err) {
		dev_err(&info->rpdev->dev, "rpmsg_send failed: %d\n", err);
		goto err_out;
	}
	err = wait_for_completion_timeout(&info->cmd_complete,
					  msecs_to_jiffies(RPMSG_TIMEOUT));
	if (!err) {
		dev_err(&info->rpdev->dev, "rpmsg_send timeout!\n");
		err = -ETIMEDOUT;
		goto err_out;
	}

	err = 0;

err_out:
	pm_qos_remove_request(&info->pm_qos_req);
	mutex_unlock(&info->lock);

	return err;
}

static int rpmsg_charger_cb(struct rpmsg_device *rpdev, void *data, int len,
			      void *priv, u32 src)
{
	struct pf1550_chg_rpmsg_pkg *msg = (struct pf1550_chg_rpmsg_pkg *)data;

	dev_dbg(&rpdev->dev,
		"recv (cb) from:%u cmd:%u, reg:0x%x, resp:%u, val:0x%x, stat:0x%x\n",
		src, msg->header.cmd, msg->reg, msg->response, msg->val, msg->status);

	if (msg->header.type == PMIC_RPMSG_REPLY) {
		chg_rpmsg.msg = msg;
		complete(&chg_rpmsg.cmd_complete);
	} else if (msg->header.type == PMIC_RPMSG_NOTIFY &&
			msg->reg == PF1550_CHARG_REG_CHG_INT) {
		/* handler charger interrupts */
		pf1550_charger_irq_handler(chg_rpmsg.charger, msg->status, msg->val);
	} else {
		dev_err(&rpdev->dev, "unknown rpmsg for pmic charger\n");
	}

	return 0;
}

static int rpmsg_charger_probe(struct rpmsg_device *rpdev)
{
	chg_rpmsg.rpdev = rpdev;

	init_completion(&chg_rpmsg.cmd_complete);
	mutex_init(&chg_rpmsg.lock);

	dev_info(&rpdev->dev, "new channel: 0x%x -> 0x%x!\n",
			rpdev->src, rpdev->dst);
	return 0;
}

static void rpmsg_charger_remove(struct rpmsg_device *rpdev)
{
	dev_info(&rpdev->dev, "rpmsg regulator driver is removed\n");
}

static struct rpmsg_device_id rpmsg_charger_id_table[] = {
	{ .name	= "rpmsg-charger-channel" },
	{ },
};

static struct rpmsg_driver rpmsg_charger_driver = {
	.drv.name	= "charger_rpmsg",
	.drv.owner	= THIS_MODULE,
	.id_table	= rpmsg_charger_id_table,
	.probe		= rpmsg_charger_probe,
	.callback	= rpmsg_charger_cb,
	.remove		= rpmsg_charger_remove,
};

static int pf1550_reg_read(struct pf1550_chg_rpmsg *info,
			unsigned int reg, unsigned int *val)
{
	struct pf1550_chg_rpmsg_pkg msg;
	int err;

	msg.header.cmd = PF1550_GET_REG;
	msg.reg = reg;
	msg.val = 0;

	err = pf1550_send_message(&msg, info);
	if (err)
		return err;

	if (info->msg->response != PF1550_SUCCESS) {
		dev_err(info->dev, "read register failed %x!\n", reg);
		return -EINVAL;
	}

	*val = info->msg->val;
	return 0;
}

static int pf1550_enable_int_recv(struct pf1550_chg_rpmsg *info, unsigned int reg)
{
	struct pf1550_chg_rpmsg_pkg msg;
	int err;

	msg.header.cmd = PF1550_RECV_INT;
	msg.reg = (unsigned char)reg;

	err = pf1550_send_message(&msg, info);
	if (err)
		return err;

	if (info->msg->response != PF1550_SUCCESS) {
		dev_err(info->dev, "write register failed %x!\n", reg);
		return -EINVAL;
	}

	return 0;
}

static int pf1550_get_charger_state(struct pf1550_chg_rpmsg *info, int *val)
{
	int ret;
	unsigned int data;

	ret = pf1550_reg_read(info, PF1550_CHARG_REG_CHG_SNS, &data);
	if (ret < 0)
		return ret;

	data &= PF1550_CHG_SNS_MASK;

	switch (data) {
	case PF1550_CHG_PRECHARGE:
	case PF1550_CHG_CONSTANT_CURRENT:
	case PF1550_CHG_CONSTANT_VOL:
	case PF1550_CHG_EOC:
		*val = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case PF1550_CHG_DONE:
		*val = POWER_SUPPLY_STATUS_FULL;
		break;
	case PF1550_CHG_TIMER_FAULT:
	case PF1550_CHG_SUSPEND:
		*val = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case PF1550_CHG_OFF_INV:
	case PF1550_CHG_OFF_TEMP:
	case PF1550_CHG_LINEAR_ONLY:
		*val = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:
		*val = POWER_SUPPLY_STATUS_UNKNOWN;
	}

	return 0;
}

static int pf1550_get_charge_type(struct pf1550_chg_rpmsg *info, int *val)
{
	int ret;
	unsigned int data;

	ret = pf1550_reg_read(info, PF1550_CHARG_REG_CHG_SNS, &data);
	if (ret < 0)
		return ret;

	data &= PF1550_CHG_SNS_MASK;

	switch (data) {
	case PF1550_CHG_SNS_MASK:
		*val = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	case PF1550_CHG_CONSTANT_CURRENT:
	case PF1550_CHG_CONSTANT_VOL:
	case PF1550_CHG_EOC:
		*val = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case PF1550_CHG_DONE:
	case PF1550_CHG_TIMER_FAULT:
	case PF1550_CHG_SUSPEND:
	case PF1550_CHG_OFF_INV:
	case PF1550_CHG_BAT_OVER:
	case PF1550_CHG_OFF_TEMP:
	case PF1550_CHG_LINEAR_ONLY:
		*val = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	default:
		*val = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}

	return 0;
}

/*
 * Supported health statuses:
 *  - POWER_SUPPLY_HEALTH_DEAD
 *  - POWER_SUPPLY_HEALTH_GOOD
 *  - POWER_SUPPLY_HEALTH_OVERVOLTAGE
 *  - POWER_SUPPLY_HEALTH_UNKNOWN
 */
static int pf1550_get_battery_health(struct pf1550_chg_rpmsg *info, int *val)
{
	int ret;
	unsigned int data;

	ret = pf1550_reg_read(info, PF1550_CHARG_REG_BATT_SNS, &data);
	if (ret < 0)
		return ret;

	data &= PF1550_BAT_SNS_MASK;

	switch (data) {
	case PF1550_BAT_NO_DETECT:
		*val = POWER_SUPPLY_HEALTH_DEAD;
		break;
	case PF1550_BAT_NO_VBUS:
	case PF1550_BAT_LOW_THAN_PRECHARG:
	case PF1550_BAT_CHARG_FAIL:
	case PF1550_BAT_HIGH_THAN_PRECHARG:
		*val = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case PF1550_BAT_OVER_VOL:
		*val = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		break;
	default:
		*val = POWER_SUPPLY_HEALTH_UNKNOWN;
		break;
	}

	return 0;
}

/* get battery voltage by RPMSG */
static int pf1550_get_battery_voltage(struct pf1550_chg_rpmsg *info, int *val)
{
	struct pf1550_chg_rpmsg_pkg msg;
	int err;

	msg.header.cmd = PF1550_BATT_VOL;
	msg.reg = 0;
	msg.val = 0;

	err = pf1550_send_message(&msg, info);
	if (err)
		return err;

	if (info->msg->response != PF1550_SUCCESS) {
		dev_err(info->dev, "read battery voltage failed!\n");
		return -EINVAL;
	}

	*val = info->msg->val;
	return 0;
}

static int pf1550_get_present(struct pf1550_chg_rpmsg *info, int *val)
{
	unsigned int data;
	int ret;

	ret = pf1550_reg_read(info, PF1550_CHARG_REG_BATT_SNS, &data);
	if (ret < 0)
		return ret;

	data &= PF1550_BAT_SNS_MASK;
	*val = (data == PF1550_BAT_NO_DETECT) ? 0 : 1;

	return 0;
}

static int pf1550_get_online(struct pf1550_chg_rpmsg *info, int *val)
{
	unsigned int data;
	int ret;

	ret = pf1550_reg_read(info, PF1550_CHARG_REG_VBUS_SNS, &data);
	if (ret < 0)
		return ret;

	*val = (data & PF1550_VBUS_VALID) ? 1 : 0;

	return 0;
}

static void pf1550_chg_bat_isr(const struct device *dev, unsigned int batsns)
{
	switch (batsns & PF1550_BAT_SNS_MASK) {
	case PF1550_BAT_NO_VBUS:
		dev_warn(dev, "No valid VBUS input.\n");
		break;
	case PF1550_BAT_LOW_THAN_PRECHARG:
		dev_warn(dev, "VBAT < VPRECHG.LB.\n");
		break;
	case PF1550_BAT_CHARG_FAIL:
		dev_warn(dev, "Battery charging failed.\n");
		break;
	case PF1550_BAT_HIGH_THAN_PRECHARG:
		dev_warn(dev, "VBAT > VPRECHG.LB.\n");
		break;
	case PF1550_BAT_OVER_VOL:
		dev_warn(dev, "VBAT > VBATOV.\n");
		break;
	case PF1550_BAT_NO_DETECT:
		dev_warn(dev, "Battery not detected.\n");
		break;
	default:
		dev_err(dev, "Unknown value read:%x\n",
			batsns & PF1550_CHG_SNS_MASK);
	}
}

static void pf1550_chg_chg_isr(const struct device *dev, unsigned int chgsns)
{

	switch (chgsns & PF1550_CHG_SNS_MASK) {
	case PF1550_CHG_PRECHARGE:
		dev_info(dev, "In pre-charger mode.\n");
		break;
	case PF1550_CHG_CONSTANT_CURRENT:
		dev_info(dev, "In fast-charge constant current mode.\n");
		break;
	case PF1550_CHG_CONSTANT_VOL:
		dev_info(dev, "In fast-charge constant voltage mode.\n");
		break;
	case PF1550_CHG_EOC:
		dev_info(dev, "In EOC mode.\n");
		break;
	case PF1550_CHG_DONE:
		dev_info(dev, "In DONE mode.\n");
		break;
	case PF1550_CHG_TIMER_FAULT:
		dev_info(dev, "In timer fault mode.\n");
		break;
	case PF1550_CHG_SUSPEND:
		dev_info(dev, "In thermistor suspend mode.\n");
		break;
	case PF1550_CHG_OFF_INV:
		dev_info(dev, "Input invalid, charger off.\n");
		break;
	case PF1550_CHG_BAT_OVER:
		dev_info(dev, "Battery over-voltage.\n");
		break;
	case PF1550_CHG_OFF_TEMP:
		dev_info(dev, "Temp high, charger off.\n");
		break;
	case PF1550_CHG_LINEAR_ONLY:
		dev_info(dev, "In Linear mode, not charging.\n");
		break;
	default:
		dev_err(dev, "Unknown value read:%x\n",
			chgsns & PF1550_CHG_SNS_MASK);
	}
}

static void pf1550_chg_vbus_isr(struct pf1550_charger *chg, unsigned int vbussns)
{
	enum power_supply_type old_type;

	old_type = chg->psy_desc.type;

	if (vbussns & PF1550_VBUS_UVLO) {
		chg->psy_desc.type = POWER_SUPPLY_TYPE_BATTERY;
		dev_dbg(chg->dev, "VBUS deattached.\n");
	}
	if (vbussns & PF1550_VBUS_IN2SYS)
		dev_dbg(chg->dev, "VBUS_IN2SYS_SNS.\n");
	if (vbussns & PF1550_VBUS_OVLO)
		dev_dbg(chg->dev, "VBUS_OVLO_SNS.\n");
	if (vbussns & PF1550_VBUS_VALID) {
		chg->psy_desc.type = POWER_SUPPLY_TYPE_MAINS;
		dev_dbg(chg->dev, "VBUS attached.\n");
	}

	if (old_type != chg->psy_desc.type)
		power_supply_changed(chg->charger);
}

static void pf1550_charger_irq_handler(struct pf1550_charger *chg,
		unsigned char status, unsigned int value)
{
	/* Do not have to clear the CHG_INT status, M4 did for us */
	if (status & CHARG_IRQ_BAT2SOCI)
		dev_info(chg->dev, "BAT to SYS Overcurrent interrupt.\n");
	if (status & CHARG_IRQ_BATI)
		pf1550_chg_bat_isr(chg->dev, (value >> 16) & 0xF);
	if (status & CHARG_IRQ_CHGI)
		pf1550_chg_chg_isr(chg->dev, (value >> 8) & 0xF);
	if (status & CHARG_IRQ_VBUSI)
		pf1550_chg_vbus_isr(chg, value & 0xF);
	if (status & CHARG_IRQ_THMI)
		dev_info(chg->dev, "Thermal interrupt.\n");
}

static enum power_supply_property pf1550_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static int pf1550_charger_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct pf1550_charger *chg = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = pf1550_get_charger_state(chg->rpmsg_info, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		ret = pf1550_get_charge_type(chg->rpmsg_info, &val->intval);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = pf1550_get_battery_health(chg->rpmsg_info, &val->intval);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		ret = pf1550_get_present(chg->rpmsg_info, &val->intval);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		ret = pf1550_get_online(chg->rpmsg_info, &val->intval);
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = pf1550_charger_model;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = pf1550_charger_manufacturer;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = pf1550_get_battery_voltage(chg->rpmsg_info, &val->intval);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int pf1550_dt_init(struct device *dev, struct pf1550_charger *chg)
{
	struct device_node *np = dev->of_node;

	if (!np) {
		dev_err(dev, "no charger OF node\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "fsl,constant-microvolt",
			&chg->constant_volt))
		chg->constant_volt = PF1550_DEFAULT_CONSTANT_VOLT;

	if (of_property_read_u32(np, "fsl,min-system-microvolt",
			&chg->min_system_volt))
		chg->min_system_volt = PF1550_DEFAULT_MIN_SYSTEM_VOLT;

	if (of_property_read_u32(np, "fsl,thermal-regulation",
			&chg->thermal_regulation_temp))
		chg->thermal_regulation_temp = PF1550_DEFAULT_THERMAL_TEMP;

	return 0;
}

static int pf1550_charger_probe(struct platform_device *pdev)
{
	struct pf1550_charger *chg;
	struct power_supply_config psy_cfg = {};
	int ret;

	chg = devm_kzalloc(&pdev->dev, sizeof(*chg), GFP_KERNEL);
	if (!chg)
		return -ENOMEM;

	chg->dev = &pdev->dev;

	platform_set_drvdata(pdev, chg);

	ret = pf1550_dt_init(&pdev->dev, chg);
	if (ret)
		return ret;


	psy_cfg.drv_data = chg;

	chg->psy_desc.name = PF1550_CHARGER_NAME;
	chg->psy_desc.type = POWER_SUPPLY_TYPE_BATTERY;
	chg->psy_desc.get_property = pf1550_charger_get_property;
	chg->psy_desc.properties = pf1550_charger_props;
	chg->psy_desc.num_properties = ARRAY_SIZE(pf1550_charger_props);

	chg->charger = power_supply_register(&pdev->dev, &chg->psy_desc,
						&psy_cfg);
	if (IS_ERR(chg->charger)) {
		dev_err(&pdev->dev, "failed: power supply register\n");
		ret = PTR_ERR(chg->charger);
		return ret;
	}
        /* attach rpmsg data onto pdev */
	chg->rpmsg_info = &chg_rpmsg;
	chg_rpmsg.charger = chg;

	/* ask M4 to deliver the interrupt events */
	ret = pf1550_enable_int_recv(&chg_rpmsg, PF1550_CHARG_REG_CHG_INT);
	return ret;
}

static int pf1550_charger_remove(struct platform_device *pdev)
{
	struct pf1550_charger *chg = platform_get_drvdata(pdev);

	power_supply_unregister(chg->charger);

	return 0;
}

static const struct of_device_id pf1550_charger_id[] = {
	{"fsl,pf1550-charger-rpmsg",},
	{},
};

MODULE_DEVICE_TABLE(of, pf1550_charger_id);

static struct platform_driver pf1550_charger_driver = {
	.driver = {
		.name	= "pf1550-charger-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = pf1550_charger_id,
	},
	.probe		= pf1550_charger_probe,
	.remove		= pf1550_charger_remove,
};

/* register the pf1550 charger rpmsg driver */
static int __init pf1550_charger_rpmsg_init(void)
{
	return register_rpmsg_driver(&rpmsg_charger_driver);
}

module_platform_driver(pf1550_charger_driver);
module_init(pf1550_charger_rpmsg_init);

MODULE_AUTHOR("Xinyu Chen <xinyu.chen@nxp.com>");
MODULE_DESCRIPTION("PF1550 charger driver based on rpmsg");
MODULE_LICENSE("GPL");
