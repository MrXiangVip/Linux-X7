
#ifndef __HL_UART_HANDLE_H__
#define __HL_UART_HANDLE_H__

#ifdef __cplusplus
extern "C"{
#endif /* _cplusplus */

/*****************************************************************************
 �� �� ��  : com_attribute_set
 ��������  : ����com������
 �������  : int iComFd 	   
			 int iDeviceSpeed  
 �������  : ��
 �� �� ֵ  : 
*****************************************************************************/
int  com_attribute_set(int iComFd, int iDeviceSpeed);

/*****************************************************************************
 �� �� ��  : lte_up_serial_com_open
 ��������  : ��com��
 �������  : int *piComFd  
 �������  : ��
 �� �� ֵ  : 
*****************************************************************************/
int serial_com_open(int *piComFd, const char *acCom, int speed);


int serial_com_write(int iComfd, char *Buf, unsigned short BufLen, unsigned int *pmsglen);
int serial_com_read(int iComfd, char *Buf, unsigned short BufLen, unsigned int *pmsglen);

/*****************************************************************************
 �� �� ��  : serial_com_close
 ��������  : �ر�com��
 �������  : int iComFd  
 �������  : ��
 �� �� ֵ  : 
*****************************************************************************/
int serial_com_close(int iComFd);

#ifdef __cplusplus
}
#endif /* _cplusplus */

#endif

