/*
 * atsha204a driver
 *
 * Copyright (C) 2019 NXP, Inc. All Rights Reserved.
 *
 */

#include <linux/bug.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h> 
#include <asm/system_misc.h>
#include <linux/cdev.h>  
#include <linux/fs.h>  
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/module.h>
#include "atsha204a.h"
#include "sha204_helper.h"
#include "atsha204a_ioctl_code.h"

#define DBG(x...)


static int sda_gpio;
static struct i2c_client *g_atsha204a_i2c = NULL;
static int status=0;
/*
 * CRC计算，从atsha204代码库移植过来
 */
static void sha204c_calculate_crc(unsigned char length, unsigned char *data, unsigned char *crc) 
{
	unsigned char counter;
	unsigned short crc_register = 0;
	unsigned short polynom = 0x8005;
	unsigned char shift_register;
	unsigned char data_bit, crc_bit;
	for (counter = 0; counter < length; counter++) 
	{
		for (shift_register = 0x01; shift_register > 0x00; shift_register <<= 1) 
		{
			data_bit = (data[counter] & shift_register) ? 1 : 0;
			crc_bit = crc_register >> 15;
			// Shift CRC to the left by 1.
			crc_register <<= 1;
			if ((data_bit ^ crc_bit) != 0)
			{
				crc_register ^= polynom;
			}
		}
	}
	crc[0] = (unsigned char) (crc_register & 0x00FF);
	crc[1] = (unsigned char) (crc_register >> 8);
}

/*
 * CRC校验，从atsha204代码库移植过来
 */
static unsigned char sha204c_check_crc(unsigned char length, unsigned char  *data)
{
	unsigned char  crc[2];
	sha204c_calculate_crc(length-2, data, crc);
	DBG("crc: 0x%x 0x%x\n",crc[0],crc[1]);
       if((crc[0] == data[length - 2]) && (crc[1] == data[length - 1]))
       {
               DBG("crc success\n");
               return SHA204_SUCCESS;
       }
       else
       {
               DBG("crc fail\n");
               return SHA204_BAD_CRC;
       }
}

/*
 * 通过I2C向atsha204写数据，考虑到唤醒原因，scl的速率为100k，不能设置太高
 */
static int atsha204a_i2c_write(const struct i2c_client *i2c, const char reg, int count, const char *buf)
{
    struct i2c_adapter *adap=i2c->adapter;
    struct i2c_msg msg;
    int ret;
    int i;
    char *tx_buf = (char *)kzalloc(count + 1, GFP_KERNEL);

    if(!i2c)
		return -EIO;
    if(!tx_buf)
        return -ENOMEM;

    tx_buf[0] = reg;
    memcpy(tx_buf+1, buf, count); 
 
    msg.addr = i2c->addr;
    msg.flags = i2c->flags;
    msg.len = count + 1;
    msg.buf = (char *)tx_buf;
    //msg.scl_rate = 50*1000;

    DBG(" \n");
    DBG("atsha204a_i2c_write write buff data : \n");
    for(i = 0; i < count + 1; i++){
    	DBG("0x%x  ", tx_buf[i]);
    }
    DBG(" \n");
 
    ret = i2c_transfer(adap, &msg, 1); 
    kfree(tx_buf);
    return (ret == 1) ? count : ret;
 
}

/*
 * 通过I2C向atsha204读数据，考虑到唤醒原因，scl的速率为100k，不能设置太高
 */
static int atsha204a_i2c_read(struct i2c_client *i2c, char reg, int count, char *dest)
{
    int ret;
    int i;
    struct i2c_adapter *adap;
    struct i2c_msg msgs;

    if(!i2c)
		return -EIO;

    adap = i2c->adapter;		

    msgs.buf = (char *)dest;
    msgs.addr = i2c->addr;
    msgs.flags = i2c->flags | I2C_M_RD;
    msgs.len = count;
    ret = i2c_transfer(adap, &msgs, 1);

    DBG(" \n");
    DBG("atsha204a_i2c_read read buff data : \n");
    for(i = 0; i < count; i++){
	DBG("0x%x  ", msgs.buf[i]);
    }
    DBG(" \n");

	return ret;
}

static int atsha204_wakeup(struct i2c_client *client)
{
       struct i2c_msg xfer_msg[1];
       uint8_t buffer[4];
       uint8_t buf[4] = {0};
       u32 ret = -99;
       
       __u16 addr0 = client->addr;

	void __iomem *sda_addr = ioremap(0x40ac0024, 4);
	writel(0x20127, sda_addr);

	// pull down the i2c6 sda pin(pte 13) 100us > 60us for atsha204a
	gpio_set_value_cansleep(sda_gpio, 0);
	udelay(100);
	gpio_set_value_cansleep(sda_gpio, 1);

       client->addr = addr0;
       mdelay(5);

	writel(0x527, sda_addr);

       ret = i2c_master_recv(client, buffer, 2);
       DBG("i2c_receive_bytes ret is %d\n",ret);
       if((buffer[0]==0x04)&&(buffer[1]==0x11))
       {
               DBG("wakeup is ok\n");
               mdelay(1);
               return ret;
       }
       else
       {
               DBG("wakeup is fail\n");
               mdelay(1);
               return -1;
       }
       
}

/*
 * 发送atsha204操作命令，写命令的地址都为0x03
 */
static int atsha204a_send_command(struct i2c_client *i2c, struct atsha204a_command_packet command_packet)
{
	// 构建发送buffer
	unsigned char command_buffer_len = 7 + command_packet.data_len;
	unsigned char *command_buffer = (unsigned char *)kzalloc(command_buffer_len, GFP_KERNEL);

	command_buffer[0] = command_buffer_len;
	command_buffer[1] = command_packet.op_code;
	command_buffer[2] = command_packet.param1;
	command_buffer[3] = command_packet.param2[0];
	command_buffer[4] = command_packet.param2[1];

    if(command_packet.data_len > 0){
    	memcpy(command_buffer+5, command_packet.data, command_packet.data_len); 
    }

	sha204c_calculate_crc(command_buffer_len - 2, command_buffer, command_buffer + command_buffer_len - 2);

	// 通过IIC发送命令
	atsha204_wakeup(g_atsha204a_i2c);
	//msleep(15);
	atsha204a_i2c_write(i2c,0x03,command_buffer_len,command_buffer);
	kfree(command_buffer);

    return 0;
}

/*
 * 读取操作命令的response内容，读取的地址都为0x00
 */
static int atsha204a_recv_command_response(struct i2c_client *i2c, unsigned char length, unsigned char *response_data)
{
	atsha204a_i2c_read(i2c, 0x0,length,response_data);
	return (!!sha204c_check_crc(length,response_data)) ? SHA204_SUCCESS : SHA204_BAD_RESPONSE;
}

/*
 * 读取芯片内部序列号，序列号有9 Byte，前两个固定为0x01,0x23 最后一个固定为0xEE
 */
static int atsha204a_get_sn(struct i2c_client *i2c, unsigned char *sn_data)
{
	struct atsha204a_command_packet get_sn_command;
	char recev_buffer[35]  = {};

	get_sn_command.op_code   = SHA204_READ;    
	get_sn_command.param1    = SHA204_ZONE_COUNT_FLAG | SHA204_ZONE_CONFIG;  
	get_sn_command.param2[0] = 0;  
	get_sn_command.param2[1] = 0;  
	get_sn_command.data      = NULL;
	get_sn_command.data_len  = 0;
    
    atsha204a_send_command(i2c, get_sn_command);
    mdelay(5);
    if(atsha204a_recv_command_response(i2c, 35, recev_buffer) == SHA204_SUCCESS){
    	memcpy(sn_data , recev_buffer + SHA204_BUFFER_POS_DATA, 4);
		memcpy(sn_data + 4 , recev_buffer + SHA204_BUFFER_POS_DATA + 8, 5);
		return SHA204_SUCCESS;
    }
    //printk("[smdt]atsha204a_get_sn %d -f \n",__LINE__);
    return SHA204_FAIL;  
}

/*
 * 读取atsha204a的数据，对应分别有data\config\otp区，每次读取 32 Byte ,所以data长度为32
 */
static int atsha204a_run_read(struct i2c_client *i2c, unsigned char zone, unsigned char word_addr, unsigned char *data)
{
	struct atsha204a_command_packet read_data_command;
	char recev_buffer[7]  = {};

	read_data_command.op_code   = SHA204_READ;  
	read_data_command.param1    = zone;  
	read_data_command.param2[0] = word_addr;  
	read_data_command.param2[1] = 0;          
	read_data_command.data      = NULL;
	read_data_command.data_len  = 0;

    atsha204a_send_command(i2c, read_data_command);
    mdelay(5);
    if(atsha204a_recv_command_response(i2c, 35, recev_buffer) == SHA204_SUCCESS){
		memcpy(data , recev_buffer + SHA204_BUFFER_POS_DATA, 32);
		return SHA204_SUCCESS;
    }
    //printk("[smdt]atsha204a_run_read %d -f \n",__LINE__);
    return SHA204_FAIL;
}

/*
 * 读取配置区的数据
 */
static int atsha204a_get_config_data(struct i2c_client *i2c, unsigned char word_addr, unsigned char *config_data)
{
	struct atsha204a_command_packet get_slot_command;
	char recev_buffer[7]  = {};

	get_slot_command.op_code   = SHA204_READ;  
	get_slot_command.param1    = SHA204_ZONE_CONFIG;  
	get_slot_command.param2[0] = word_addr;  
	get_slot_command.param2[1] = 0;          
	get_slot_command.data      = NULL;
	get_slot_command.data_len  = 0;

    atsha204a_send_command(i2c, get_slot_command);
    mdelay(5);
    if(atsha204a_recv_command_response(i2c, 7, recev_buffer) == SHA204_SUCCESS){
    	memcpy(config_data , recev_buffer + SHA204_BUFFER_POS_DATA, 4);
		return SHA204_SUCCESS;
    }
    //printk("[smdt]atsha204a_get_config_data %d -f \n",__LINE__);
    return SHA204_FAIL;
}

/*
 * 读取slot的数据
 */
static int atsha204a_get_slot_data(struct i2c_client *i2c, unsigned char slot_id, unsigned char *slot_data)
{
	struct atsha204a_command_packet get_slot_command;
	char recev_buffer[35]  = {};

	get_slot_command.op_code   = SHA204_READ;  
	get_slot_command.param1    = SHA204_ZONE_COUNT_FLAG | SHA204_ZONE_DATA; 
	get_slot_command.param2[0] = slot_id << 2;  
	get_slot_command.param2[1] = 0;  
	get_slot_command.data      = NULL;
	get_slot_command.data_len  = 0;

    atsha204a_send_command(i2c, get_slot_command);
    mdelay(5);
    if(atsha204a_recv_command_response(i2c, 35, recev_buffer) == SHA204_SUCCESS){
    	memcpy(slot_data , recev_buffer + SHA204_BUFFER_POS_DATA, 32);
		return SHA204_SUCCESS;
    }
    //printk("[smdt]atsha204a_get_slot_data %d -f \n",__LINE__);
    return SHA204_FAIL;
}

/*
 * 读取atsha204内部生成的随机数
 */
static int atsha204a_run_random(struct i2c_client *i2c, unsigned char *random_data)
{
	struct atsha204a_command_packet get_random_command;
	char recev_buffer[35]  = {};

	get_random_command.op_code   = SHA204_RANDOM;    
	get_random_command.param1    = 0;  
	get_random_command.param2[0] = 0;  
	get_random_command.param2[1] = 0;  
	get_random_command.data      = NULL;
	get_random_command.data_len  = 0;

    atsha204a_send_command(i2c, get_random_command);
    mdelay(6);
    if(atsha204a_recv_command_response(i2c, 35, recev_buffer) == SHA204_SUCCESS){
    	memcpy(random_data , recev_buffer + SHA204_BUFFER_POS_DATA, 32);
		return SHA204_SUCCESS;
    }
    //printk("[smdt]atsha204a_run_random %d -f \n",__LINE__);
    return SHA204_FAIL;
}

/*
 * 执行 atsha204 NONCE命令,NONCE会返回32位的随机数
 */
static int atsha204a_run_nonce(struct i2c_client *i2c, unsigned char mode, unsigned char *num_in, unsigned char *nonce_data)
{
	struct atsha204a_command_packet run_nonce_command;
	char recev_buffer[35]  = {};

	run_nonce_command.op_code   = SHA204_NONCE;    
	run_nonce_command.param1    = mode;  
	run_nonce_command.param2[0] = 0;  
	run_nonce_command.param2[1] = 0;  
	run_nonce_command.data      = num_in;
	run_nonce_command.data_len  = 20;

    atsha204a_send_command(i2c, run_nonce_command);
    mdelay(30);
    if(atsha204a_recv_command_response(i2c, 35, recev_buffer) == SHA204_SUCCESS){
    	memcpy(nonce_data , recev_buffer + SHA204_BUFFER_POS_DATA, 32);
		return SHA204_SUCCESS;
    }
    //printk("[smdt]atsha204a_run_nonce %d -f \n",__LINE__);
    return SHA204_FAIL;
}

/*
 * 执行 atsha204 MAC命令，MAC命令返回sha256算法生成的摘要
 */
static int atsha204a_run_mac(struct i2c_client *i2c, unsigned char mode, unsigned char slot_id, unsigned char challenge_len, unsigned char *challenge, unsigned char *mac_data)
{
	struct atsha204a_command_packet run_mac_command;
	char recev_buffer[35]  = {};

	run_mac_command.op_code   = SHA204_MAC;  
	run_mac_command.param1    = mode;  
	run_mac_command.param2[0] = slot_id;
	run_mac_command.param2[1] = 0;
	run_mac_command.data      = challenge;
	run_mac_command.data_len  = challenge_len;

    atsha204a_send_command(i2c, run_mac_command);
    mdelay(20);
    if(atsha204a_recv_command_response(i2c, 35, recev_buffer) == SHA204_SUCCESS){
    	memcpy(mac_data , recev_buffer + SHA204_BUFFER_POS_DATA, 32);
		return SHA204_SUCCESS;
    }
    return SHA204_FAIL;
}


/*
 * 模拟加密验证的流程
 */
static int atsha204a_mac(struct i2c_client *i2c, unsigned char slot_id, unsigned char *secret_key, unsigned char num_in_len, unsigned char *num_in, unsigned char challenge_len, unsigned char *challenge)
{
	int i;
	char nonce_response[32]  = {};
	char mac_response[32]    = {};
	uint8_t soft_digest [32];						//!< Software calculated digest
	struct sha204h_nonce_in_out nonce_param;		//!< Parameter for nonce helper function
	struct sha204h_mac_in_out mac_param;			//!< Parameter for mac helper function
	struct sha204h_temp_key tempkey;				//!< tempkey parameter for nonce and mac helper function

	// atsha204a run nonce
	if (atsha204a_run_nonce(i2c,NONCE_MODE_NO_SEED_UPDATE,num_in,nonce_response) == SHA204_FAIL)	{
		DBG("atsha204a_run_nonce fail \n ");
		return SHA204_FAIL;
	}

    // simulate run nonce
    nonce_param.mode = NONCE_MODE_NO_SEED_UPDATE;
	nonce_param.num_in = num_in;	
	nonce_param.rand_out = nonce_response;	
	nonce_param.temp_key = &tempkey;
	if (sha204h_nonce(&nonce_param) == SHA204_FAIL){
		DBG("sha204h_nonce fail \n ");
		return SHA204_FAIL;
	}

	// atsha204 run mac
	if (atsha204a_run_mac(i2c, MAC_MODE_BLOCK2_TEMPKEY, slot_id, challenge_len, challenge,mac_response) == SHA204_FAIL)	{
		DBG("atsha204a_run_mac fail \n ");
		return SHA204_FAIL;
	}
	
	// simulate run mac
    mac_param.mode = MAC_MODE_BLOCK2_TEMPKEY;
	mac_param.key_id = slot_id;
	mac_param.challenge = challenge;
	mac_param.key = secret_key;
	mac_param.otp = NULL;
	mac_param.sn = NULL;
	mac_param.response = soft_digest;
	mac_param.temp_key = &tempkey;
	if (sha204h_mac(&mac_param) == SHA204_FAIL){
		DBG("sha204h_mac fail \n ");
		return SHA204_FAIL;
	}
    
	// compare result
    if(memcmp(soft_digest,mac_response,32) == 0){
    	DBG(" mac succ ! \n");
    }else{
		DBG(" mac fail ! \n");
	}

    return SHA204_SUCCESS;
}

/*
 * Probe中读取加密芯片的序列号进行判断芯片是否存在
 */
static ssize_t atsha_status_show(struct device *dev,
                                         struct device_attribute *attr, char *buf)
{
        if(1==status)
			return sprintf(buf, "%s\n","success");
		else
			return sprintf(buf, "%s\n","failed");
}

static ssize_t atsha_status_store(struct device *dev,
					  struct device_attribute *attr, const char *buf,
								  size_t size)
{        
       return size;	
}
static DEVICE_ATTR_RW(atsha_status);

static struct attribute *atsha_attrs[] = {
		&dev_attr_atsha_status.attr,
		NULL,
};
ATTRIBUTE_GROUPS(atsha);









static int atsha204a_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct device *dev = &i2c->dev;
	int retval;
	unsigned char sn_data[9] = {0x00};
	void __iomem *sda_addr = ioremap(0x40ac0024, 4);
	
	g_atsha204a_i2c = i2c;

	writel(0x20127, sda_addr);
	// request lpi2c6 sda pin
	sda_gpio = of_get_named_gpio(dev->of_node, "sda-gpios", 0);
	if (!gpio_is_valid(sda_gpio)) {
	dev_err(dev, "no lpi2c6 sda pin available\n");
		return -EINVAL;
	}
	retval = devm_gpio_request_one(dev, sda_gpio, GPIOF_OUT_INIT_HIGH,
					"lpi2c6_sda");
	if (retval < 0)
		return retval;

	gpio_set_value_cansleep(sda_gpio, 1);

	writel(0x527, sda_addr);

	// 读取加密芯片序列号进行判断
	if(atsha204a_get_sn(g_atsha204a_i2c, sn_data) == SHA204_SUCCESS){
		if(sn_data[0] == 0x01 && sn_data[1] == 0x23 && sn_data[8] == 0xEE){
			printk(" atsha204a_probe succ ! \n");
			DBG(" atsha204a_probe succ ! \n");
			status = 1;
			return 0;
		}
	}
	
	printk(" atsha204a_probe fail -- no atsha204a found ! \n");
	return -1;
}
 
static int  atsha204a_remove(struct i2c_client *i2c){
	return 0;
}

static const struct of_device_id atsha204a_of_match[] = {
	{ .compatible = "atmel,atsha204a"},
};

static const struct i2c_device_id atsha204a_id[] = {
       { "atsha204a", 0 },
       { }
};  
   
MODULE_DEVICE_TABLE(i2c, atsha204a_id);

static struct i2c_driver atsha204a_driver = {
	.driver = {
		.name = "atsha204a",
		.owner = THIS_MODULE, 
		.of_match_table =of_match_ptr(atsha204a_of_match),
	},
	.probe    = atsha204a_probe,
	.remove   = atsha204a_remove,
	.id_table = atsha204a_id,
};
  

/***************************** driver device file for atsha204a control ******************************/

static int atsha204a_open(struct inode* inode, struct file* filp) {      
	DBG("atsha204a_open. \n");
    return 0;  
}  
   
static int atsha204a_release(struct inode* inode, struct file* filp) {  
	DBG("atsha204a_release. \n");
    return 0;  
}  

static ssize_t atsha204a_read(struct file *filp, char __user *buf, size_t sz, loff_t *off)  {  
	DBG("atsha204a_read. \n");
    return sizeof(int);  
}  
  
static ssize_t atsha204a_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos)  {    
	DBG("atsha204a_write. \n");
    return sizeof(int);  
}  

static long atsha204a_ioctl(struct file* filp, unsigned int cmd, unsigned long data)  {    
 	struct atsha204a_sn sn;
	struct atsha204a_nonce nonce;
	struct atsha204a_mac mac;
	struct atsha204a_read read;
	int ret=0;
	DBG("atsha204a_ioctl. %d\n" , cmd);
 
	switch (cmd) {
	case ATSHA204A_SN_CMD:		
		//ret=atsha204a_get_sn(g_atsha204a_i2c, &sn.out_response);
		ret=atsha204a_get_sn(g_atsha204a_i2c, sn.out_response);
		copy_to_user(((struct atsha204a_sn*)data)->out_response, sn.out_response, sizeof(sn.out_response));
		return ret;
	case ATSHA204A_NONCE_CMD:		
		copy_from_user(&nonce, ((struct atsha204a_nonce*)data), sizeof(nonce));
		ret=atsha204a_run_nonce(g_atsha204a_i2c, nonce.in_mode, nonce.in_numin, nonce.out_response);		
		copy_to_user(((struct atsha204a_nonce*)data)->out_response, nonce.out_response, sizeof(nonce.out_response));
	        return ret;
	case ATSHA204A_MAC_CMD:		
		copy_from_user(&mac, ((struct atsha204a_nonce*)data), sizeof(mac));
		ret=atsha204a_run_mac(g_atsha204a_i2c, mac.in_mode, mac.in_slot, mac.in_challenge_len, mac.in_challenge, mac.out_response);
		copy_to_user(((struct atsha204a_mac*)data)->out_response, mac.out_response, sizeof(mac.out_response));
		return ret;
	case ATSHA204A_WRITE_CMD:
		return 1;
	case ATSHA204A_READ_CMD:		
		copy_from_user(&read, ((struct atsha204a_read*)data), sizeof(read));
		ret=atsha204a_run_read(g_atsha204a_i2c, read.in_zone, read.in_word_addr, read.out_response);
		copy_to_user(((struct atsha204a_read*)data)->out_response, read.out_response, sizeof(read.out_response));
		return ret;
	default:
		return 0;
	}
    return 0;  
} 


static int    atsha204a_major = 0;
static int    atsha204a_minor = 0;
static dev_t  ndev; 
static struct device *atsha204a_device = NULL;
static struct class *crypto_class      = NULL;  
static struct cdev *atsha204a_dev      = NULL;  
static struct file_operations atsha204a_driver_fops = {
    .owner   = THIS_MODULE,
    .open    = atsha204a_open,  
    .release = atsha204a_release, 
    .read    = atsha204a_read,  
    .write   = atsha204a_write, 
    .unlocked_ioctl   = atsha204a_ioctl, 
    .compat_ioctl=atsha204a_ioctl, 
};

/*
 * 驱动模块加载初始化
 */
static int __init atsha204a_module_init(void)
{
	int ret = -1; 
	dev_t devno;

	ret = i2c_add_driver(&atsha204a_driver);
	if (ret != 0){
		pr_err("failed to register I2C driver: %d\n", ret);
		goto clear_return;
	}

    // 设备号相关 
    ret = alloc_chrdev_region(&ndev, 0, 1, "atsha204a");
    if(ret < 0){
    	pr_err("failed to alloc chrdev : %d\n", ret);
    	goto destroy_i2c;
    } 
    atsha204a_major = MAJOR(ndev);  
    atsha204a_minor = MINOR(ndev);  

	// 分配atsha204a设备结构体变量
    atsha204a_dev = cdev_alloc();
    if(!atsha204a_dev) { 
    	ret = -ENOMEM; 
        pr_err(KERN_ALERT"failed to alloc mem for atsha204a_dev \n");  
        goto unregister_chrdev;
    }  

    // 初始化注册设备
    devno = MKDEV(atsha204a_major, atsha204a_minor);  
    cdev_init(atsha204a_dev, &atsha204a_driver_fops);  
    atsha204a_dev->ops = &atsha204a_driver_fops;

    ret = cdev_add(atsha204a_dev,devno, 1);  
    if (ret){
        pr_err(KERN_ALERT"failed to add char dev for atsha204a \n");  
        goto unregister_chrdev;
    } 

	// 注册一个类，使mdev可以在"/sys/class"目录下 面建立设备节点 
    crypto_class = class_create(THIS_MODULE, "crypto");
    if (IS_ERR(crypto_class)){
        ret = -1;
        DBG("failed to create atsha204a moudle class.\n");
        goto destroy_cdev;
    }

    // 创建一个设备节点，节点名为atsha204a ,如果成功，它将会在/dev/atsha204a设备。
    atsha204a_device = device_create(crypto_class, NULL, MKDEV(atsha204a_major, 0), "atsha204a", "atsha204a");

    if (IS_ERR(atsha204a_device)){
    	ret = -1;
        DBG("failed to create atsha204a device .\n");
        goto destroy_class;
    }

    return 0;

destroy_device:  
    device_destroy(crypto_class, ndev);  
  
destroy_class:  
    class_destroy(crypto_class);  
  
destroy_cdev:  
    cdev_del(atsha204a_dev); 

unregister_chrdev:  
    unregister_chrdev_region(ndev, 1);

destroy_i2c:  
    i2c_del_driver(&atsha204a_driver); 

clear_return:
    return ret;
} 

static void __exit atsha204a_module_exit(void)
{
	device_destroy(crypto_class, ndev);  
	class_destroy(crypto_class);  
	cdev_del(atsha204a_dev); 
	unregister_chrdev_region(ndev, 1);
	i2c_del_driver(&atsha204a_driver); 
}

module_init(atsha204a_module_init);
module_exit(atsha204a_module_exit);

MODULE_LICENSE("GPL");
