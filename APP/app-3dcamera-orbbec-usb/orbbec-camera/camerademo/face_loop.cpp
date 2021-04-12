/*
 * Copyright 2018 NXP Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <iostream>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

#include "RSApiHandle.h"

//#define LCD_DISPLAY_ENABLE  //hannah added 0220

typedef enum
{
	RPMSG_CMD,
	RPMSG_READ,
}para_rpmsg_ioctl_cmd;

typedef enum
{
    CMD_INVALID = 0U,
    CMD_FACE_RECOGNITION = 1U,
    CMD_FACE_REGISTRATION = 2U, 
	CMD_FACE_DELETION = 3U,        
}face_cmd_type_t;

int faceDeleteProc(int faceId)
{
	RSApiHandle rsHandle;
	int ret = -1;

	if(!rsHandle.Init()) {
		cerr << "Failed to init the ReadSense lib" << endl;
		return ret;  //-1: RS_RET_UNKOWN_ERR
	}	
					
	ret = rsHandle.DeleteFace(faceId);
	if(ret == RS_RET_OK)  //0: RS_RET_OK
		cout << "face deleted: id  "<< faceId << endl;
	
	return ret;
}

int main(int argc, char **argv)
{
	int ret=-1, rpmsg_fd=-1, val=0;
	unsigned char rpmsg_buf[4] = {0};	
	const char *rpmsgdev = "/dev/ttyRPMSG30";	
	int faceId, len, fd; 
	unsigned char DelResp[4] = {0x41, 0x37, 0x03, 0x00};  //'A' '7' cmd faceId	
	pid_t status;
	
	while(1)
	{
		rpmsg_fd = open(rpmsgdev, O_RDWR);
		if(!rpmsg_fd)
			printf("ttyRPMSG30 open failed!\n");
		ret = ioctl(rpmsg_fd, RPMSG_READ, &val);  //transfer val addr to kernel
		//ret = ioctl(rpmsg_fd, RPMSG_READ, (unsigned long)user_arg);  //not applicable
		if(ret == 0)
		{		
		//	printf("\nval(user) = 0x%08X\n", val);
			close(rpmsg_fd);
			memcpy(rpmsg_buf, &val, 4);					
		}
		else
		{
			printf("rpmsg ioctl error: %d!\n", ret);
			break;
		}		

		//printf("rpmsg_buf(user): 0x %02x %02x %02x %02x\n", rpmsg_buf[0], rpmsg_buf[1], rpmsg_buf[2], rpmsg_buf[3]);		
		if((rpmsg_buf[0] == 0x4d) && (rpmsg_buf[1] == 0x34))
		{
			printf("rpmsg_buf(user): 0x %02x %02x %02x %02x\n", rpmsg_buf[0], rpmsg_buf[1], rpmsg_buf[2], rpmsg_buf[3]);			
			switch(rpmsg_buf[2])
			{
				case CMD_FACE_RECOGNITION:
					cout << "Switch to face recognize..." << endl;
					system("/opt/orbbec-camera/face_recg.sh");
					break;
				case CMD_FACE_REGISTRATION:
					cout << "Switch to face register..." << endl;	
					system("/opt/orbbec-camera/face_register.sh");
					break;
				case CMD_FACE_DELETION:			
					faceId = (int)rpmsg_buf[3];
					if(faceId < 0xFF)  // < 0x100 for kds
					{
						cout << "Delete specific face ID ..." << endl;						
						ret = faceDeleteProc(faceId);
						if(ret == RS_RET_OK)
							DelResp[3] = (unsigned char)faceId;
						else
						{
							DelResp[3] = 0xFF;  //delete ID fail
							cout << "Face ID delete fail!" << endl;
						}
					}
					else if(faceId == 0xFF)  //delete the database
					{	
						cout << "Delete the database file!!!" << endl;
						status = system("rm /opt/orbbec-camera/readsense.db");
						if(0 == WEXITSTATUS(status))
							DelResp[3] = 0x00;  //cmd run success
						else
						{
							DelResp[3] = 0xFF;  //delete fail
							cout << "Face database delete fail!" << endl;
						}
					}
					system("sync");

					len = sizeof(DelResp);
					fd = open(rpmsgdev, O_RDWR);  //open RPmsg virtual tty device		
					if(fd > 0)
					{
						write(fd, (unsigned char *)DelResp, len);
						close(fd);  //close fd after RW operation
					}
					else
						printf("open /dev/ttyRPMSG30 failed\n");
				#ifndef LCD_DISPLAY_ENABLE
					system("echo \"Face Deletion done, A7 enter VLLS!\""); 
					system("echo mem > /sys/power/state");
				#endif	
					break;
				default:
					cout << "No effective cmd from M4!" << endl;
					break;				
			}
		}
	usleep(50000);  //sleep 50ms
	}	

	return 0;
}

