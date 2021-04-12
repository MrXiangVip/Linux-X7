#include <stdio.h>
#include <stdlib.h>
#include <string.h>  
#include <errno.h>  
#include <sys/stat.h>   
#include <unistd.h>  
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include "uart-handle.h"
#include "log.h"

#define DevSpeed			B115200

/*****************************************************************************
 �� �� ��  : com_attribute_set
 ��������  : ����com������
 �������  : int iComFd 	   
			 int iDeviceSpeed  
 �������  : ��
 �� �� ֵ  : 
*****************************************************************************/
int  com_attribute_set(int iComFd, int iDeviceSpeed)
{
	struct termios TtyAttr;
	int DeviceSpeed = DevSpeed;
	int ByteBits = CS8;

	DeviceSpeed = iDeviceSpeed;

	memset(&TtyAttr, 0, sizeof(struct termios));
	TtyAttr.c_iflag = IGNPAR;
	TtyAttr.c_cflag = DeviceSpeed | HUPCL | ByteBits | CREAD | CLOCAL;
	TtyAttr.c_cc[VMIN] = 1;
	
	/* ��com������Ϊ������ģʽ */
	if (fcntl(iComFd, F_SETFL, O_NONBLOCK) < 0)
	{
		printf("update com fcntl failed\n"); 
		return -1;
	}
	/* ����com�ڵĲ����ʵ����� */
	if (tcsetattr(iComFd, TCSANOW, &TtyAttr) < 0)
	{
		printf("update com attr set failed\n");
		return -1;
	}
	return 0;
}

/*****************************************************************************
 �� �� ��  : lte_up_serial_com_open
 ��������  : ��com��
 �������  : int *piComFd  
 �������  : ��
 �� �� ֵ  : 
*****************************************************************************/
int serial_com_open(int *piComFd,const char *acCom, int speed)
{	 
	// int iDeviceSpeed = DevSpeed;
	int iDeviceSpeed = speed;
	int iComFd = 0; /* ��lte up com�ں��Ӧ��fd */
	
	if(NULL == piComFd)
	{
		log_warn("param [piComFd] is NULL\n");
		*piComFd = -1;
		return -1;
	}
	
	/* �򿪴�����Ҫ�������O_NOCTTY�������κ����붼��Ӱ����� */
	if ((iComFd = open(acCom, O_RDWR | O_NOCTTY)) < 0) 
	{
		perror("");
		log_warn("open lte up com [%s] failed\n", acCom);
		*piComFd = -1;
		return -1;
	}

	/*����COM�ڵ�����*/
	if (-1 == com_attribute_set(iComFd, iDeviceSpeed))
	{
		*piComFd = -1;
		/* ����com�����ԣ�ʧ����ر�com�� */
		close(iComFd);
		return -1;
	}

	*piComFd = iComFd;
	log_info("open lte up com [%s] success\n", acCom);

	return 0;
}

int serial_com_write(int iComfd, char *Buf, unsigned short BufLen, unsigned int *pmsglen)
{
	int iWriteBytes = 0;
	iWriteBytes = write(iComfd, Buf, BufLen);
	if(iWriteBytes < 0)
	{
		log_warn("write filesize to com failed!\n");
		// printf("write filesize to com failed!\n");
		serial_com_close(iComfd);
		return -1;
	}
	// else
	// 	printf("write filesize len=%d.\n",iWriteBytes);

	*pmsglen = iWriteBytes;
	// tcflush(iComfd, TCIFLUSH);
	
	return 0;
}

int serial_com_read(int iComfd, char *Buf, unsigned short BufLen, unsigned int *pmsglen)
{
	int iReadBytes = 0;
	memset(Buf, '\0', BufLen);
	iReadBytes = read(iComfd, Buf, BufLen);
	if(iReadBytes < 0)
	{
		return -1;
	}
	
	*pmsglen = iReadBytes;
	// tcflush(iComfd, TCOFLUSH);
	
	return 0;
}

/*****************************************************************************
 �� �� ��  : serial_com_close
 ��������  : �ر�com��
 �������  : int iComFd  
 �������  : ��
 �� �� ֵ  : 
*****************************************************************************/
int serial_com_close(int iComFd)
{
	if(iComFd < 0)
	{
		printf("lte up iComFd had been close!\n");
	}
	else
	{
		close(iComFd);
	}
	return 0;
}


