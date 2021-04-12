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
 函 数 名  : com_attribute_set
 功能描述  : 设置com口属性
 输入参数  : int iComFd 	   
			 int iDeviceSpeed  
 输出参数  : 无
 返 回 值  : 
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
	
	/* 将com口设置为非阻塞模式 */
	if (fcntl(iComFd, F_SETFL, O_NONBLOCK) < 0)
	{
		printf("update com fcntl failed\n"); 
		return -1;
	}
	/* 设置com口的波特率等属性 */
	if (tcsetattr(iComFd, TCSANOW, &TtyAttr) < 0)
	{
		printf("update com attr set failed\n");
		return -1;
	}
	return 0;
}

/*****************************************************************************
 函 数 名  : lte_up_serial_com_open
 功能描述  : 打开com口
 输入参数  : int *piComFd  
 输出参数  : 无
 返 回 值  : 
*****************************************************************************/
int serial_com_open(int *piComFd,const char *acCom, int speed)
{	 
	// int iDeviceSpeed = DevSpeed;
	int iDeviceSpeed = speed;
	int iComFd = 0; /* 打开lte up com口后对应的fd */
	
	if(NULL == piComFd)
	{
		log_warn("param [piComFd] is NULL\n");
		*piComFd = -1;
		return -1;
	}
	
	/* 打开串口需要添加属性O_NOCTTY，否则任何输入都会影响进程 */
	if ((iComFd = open(acCom, O_RDWR | O_NOCTTY)) < 0) 
	{
		perror("");
		log_warn("open lte up com [%s] failed\n", acCom);
		*piComFd = -1;
		return -1;
	}

	/*设置COM口的属性*/
	if (-1 == com_attribute_set(iComFd, iDeviceSpeed))
	{
		*piComFd = -1;
		/* 设置com口特性，失败则关闭com口 */
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
 函 数 名  : serial_com_close
 功能描述  : 关闭com口
 输入参数  : int iComFd  
 输出参数  : 无
 返 回 值  : 
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


