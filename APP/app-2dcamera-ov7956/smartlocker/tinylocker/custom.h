/*
description:
	This file is using to define the function enable for products.
Date:
	2019-7-12
Author:
	
 */

#ifndef __WF_CUSTOM_H__
#define __WF_CUSTOM_H__


/*===================L C D=================*/ 
#define LCD_DISPLAY_ENABLE  

#ifdef LCD_DISPLAY_ENABLE
	#define LCD_DEFINITION_480P 
	//#define LCD_DEFINITION_720P
#endif

/*===================APP UPGRADE=================*/ 
#define APP_UPGRADE_USB

#endif // __WF_CUSTOM_H__

