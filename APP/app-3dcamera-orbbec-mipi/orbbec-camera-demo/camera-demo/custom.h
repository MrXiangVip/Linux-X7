/*
description:
	This file is using to define the function enable for products.
Date:
	2019-7-12
Author:
	
 */

#ifndef __WF_CUSTOM_H__
#define __WF_CUSTOM_H__

#define SYS_VERSION		"C1L1.A2_V2.1.14"

/*===================L C D=================*/ 
#define LCD_DISPLAY_ENABLE  

#ifdef LCD_DISPLAY_ENABLE
	#define LCD_DEFINITION_480P 
	//#define LCD_DEFINITION_720P
#endif


/* 1:ʹ��RMPSG��M4����ͨ�ţ�0:ֱ��ʹ��A7 UART5ͨ��*/
#define USE_M4_FLAG		0 

/*===================APP UPGRADE=================*/ 
#define APP_UPGRADE_USB

#define ORBBEC_STREAM_M_MODE //�±�����ͷ����Ϊ�ֶ��л�ir/depth����ʽ

#endif // __WF_CUSTOM_H__

