
#ifndef __HL_UART_HANDLE_H__
#define __HL_UART_HANDLE_H__

#ifdef __cplusplus
extern "C"{
#endif /* _cplusplus */

/*****************************************************************************
 函 数 名  : com_attribute_set
 功能描述  : 设置com口属性
 输入参数  : int iComFd 	   
			 int iDeviceSpeed  
 输出参数  : 无
 返 回 值  : 
*****************************************************************************/
int  com_attribute_set(int iComFd, int iDeviceSpeed);

/*****************************************************************************
 函 数 名  : lte_up_serial_com_open
 功能描述  : 打开com口
 输入参数  : int *piComFd  
 输出参数  : 无
 返 回 值  : 
*****************************************************************************/
int serial_com_open(int *piComFd,char *acCom);


int serial_com_write(int iComfd, unsigned char *Buf, unsigned short BufLen, unsigned short *pmsglen);
int serial_com_read(int iComfd, unsigned char *Buf, unsigned short BufLen, unsigned short *pmsglen);

/*****************************************************************************
 函 数 名  : serial_com_close
 功能描述  : 关闭com口
 输入参数  : int iComFd  
 输出参数  : 无
 返 回 值  : 
*****************************************************************************/
int serial_com_close(int iComFd);

#ifdef __cplusplus
}
#endif /* _cplusplus */

#endif

