/*
description:
	This file is using to define the function enable for products.
Date:
	2019-7-12
Author:
	
 */

#ifndef __WF_CUSTOM_H__
#define __WF_CUSTOM_H__

#define SYS_VERSION		"1.0.1"

/*===================L C D=================*/ 
#define LCD_DISPLAY_ENABLE  

#ifdef LCD_DISPLAY_ENABLE
	#define LCD_DEFINITION_480P 
#endif


/* 是否使用RMPSG和M4串口通信*/
#define USE_M4_FLAG		1

/*===================APP UPGRADE=================*/ 
#define APP_UPGRADE_USB

#define ORBBEC_STREAM_M_MODE //奥比摄像头出流为手动切换ir/depth流方式

#endif // __WF_CUSTOM_H__

