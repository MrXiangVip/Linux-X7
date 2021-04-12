/**************************************************************************
 * 	FileName:	 face_loop.cpp
 *	Description:	Deal with message from M4,and upgrade system app and algorithm
 *	Copyright(C):	2018-2020 Wave Group Inc.
 *	Version:		V 1.0
 *	Author:		tanqw
 *	Created:		
 *	Updated:		2019-08-13
 *					
**************************************************************************/

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
#include "custom.h"
#include "function.h"

#include "libsimplelog.h"


int faceDeleteProc(int faceId)
{
	RSApiHandle rsHandle;
	int ret = -1;

	if(!rsHandle.Init(FACE_LOOP)) {
		cerr << "Failed to init the ReadSense lib" << endl;
		return ret;  //-1: RS_RET_UNKOWN_ERR
	}	
					
	ret = rsHandle.DeleteFace(faceId);
	if(ret == RS_RET_OK)  //0: RS_RET_OK
		cout << "face deleted: id  "<< faceId << endl;
	
	return ret;
}

//�ж��Ƿ����U��
BOOL CheckIsUDiskIn()
{
	char buf[512] = {0};
	FILE *fp = NULL;
	char* cmd1 = "ls /sys/class/scsi_device/";
	
	fp = popen( cmd1 , "r");
	fread(buf, 1, 7, fp);		
	//printf("USB drive buf now. buf = %s  \n",buf);
	pclose(fp);
		
	// δ���U����Ϣ���п�
	if(buf != NULL)
	{
		if(strlen(buf) == 0)
		{
			//printf("=======has no udisk.\n");
    			return NO;
		}
	}

	return YES;
}

int SetSysUpdateFlagToCfgFile(int Val)
{
	FILE   *pFile = NULL;
	char	szBuffer[1024] = {0};
	int iBufferSize=0;				

	memset(szBuffer, 0, sizeof(szBuffer));
	iBufferSize += sprintf(szBuffer + iBufferSize, "UpgradeFlag=%d\n", Val);

	pFile = fopen("/tmp/SysUpdate.cfg", "w");
	if (pFile)
	{
		fwrite(szBuffer, iBufferSize, 1, pFile);	
		
		/* ���ļ�ͬ��,��ʱд�����,��Ȼ��ϵͳ�������������ļ����ݶ�ʧ */
		fflush(pFile);		/* ˢ���ڴ滺�壬������д���ں˻��� */
		fsync(fileno(pFile));	/* ���ں˻���д����� */
	}
	else
	{
		return -1;
	}

	if (NULL != pFile)
	{
		fclose(pFile);
		pFile = NULL;
	}

	return 0;
}

int GetSysUpdateFlagFromCfgFile(int * pVal)
{
	FILE   *pFile = NULL;
	char   szMsg[256] = {0};
	unsigned short len = 0;
	
	if (access("/tmp/SysUpdate.cfg", F_OK) == 0)  
	{
		pFile = fopen("/tmp/SysUpdate.cfg", "r");
		if (pFile == NULL)
		{
			printf("Err, can't fopen</tmp/SysUpdate.cfg>\n");
			return -1;
		}
		memset(szMsg, 0, sizeof(szMsg));

		while (fgets(szMsg, sizeof(szMsg), pFile) != NULL)
		{
			if (strstr(szMsg, "UpgradeFlag=") != NULL) 
			{
				len = strlen("UpgradeFlag=");
				sscanf(&szMsg[len], "%d", pVal);//skip "UpgradeFlag="
			}

			memset(szMsg, 0, sizeof(szMsg));
		}
	}
	else
	{
		/*��������������ļ�����ô��ϵͳĬ�ϵ�ֵ�������������ļ�*/
		*pVal =	0;
		printf("Warring, not exit </tmp/SysUpdate.cfg>,creat it now.\n");

		SetSysUpdateFlagToCfgFile(0);
	}

	printf("UpgradeFlag=<%d>\n", *pVal);

	if (NULL != pFile)
	{
		fclose(pFile);
		pFile = NULL;
	}
	
	return 0;
}

void *SystemAppUpgrade(void *)
{
	int len; 
	unsigned char RspStr[4] = {0x41, 0x37, 0xfd, 0x00};  //'A' '7' cmd faceId	
	uint32_t crc32_val;
	FILE   *pFile = NULL;
	//printf("[FUNC:%s] starting...\n", __FUNCTION__);
	system("mdev -s");
	sleep(1);

	if (access("/run/media/sda1/Face_Update_x7.bin", F_OK) == 0)  
	{
		/*copy update file in U-disk to mian work dir of system*/
		system("rm -f /home/root/Face_Update*");
		system("cp /run/media/sda1/Face_Update_x7.bin  /home/root");
		
		if(!CheckImageFileCRC32("/home/root/Face_Update_x7.bin", "/home/root/Face_Update.bin", &crc32_val))
		{
			printf("The upgrade package<crc32:0x%lx> is OK, update system start....\n", crc32_val);

			/*��ѹ*/
			system("mkdir /home/root/update_dir");
			system("tar -xvf /home/root/Face_Update.bin -C /home/root/update_dir");
			
			/*Ԥ��һ��������������ʱִ�еĽŲ���������������һЩ��ʱ�ĳ��㷨��Ӧ��Ŀ¼�������*/
			system("/opt/smartlocker/upgrade.sh");
			sleep(1);
			
			/*���ݼ���������Ŀ¼*/
			system("tar -cvzf /tmp/face_app_bak.tar.gz  /opt/smartlocker/");
			system("cp /usr/lib/libReadFace.so   /tmp/");
			
			/*д������ǵ��ļ������滻�����жϵ���쳣ʱ��û�������ɹ����Ͱ����汸�ݵĻָ�*/
			SetSysUpdateFlagToCfgFile(1);

			/*�滻�ɵ�Ӧ�ú��㷨��*/
			system("rm -rf /opt/smartlocker/*");
			system("rm -rf /usr/lib/libReadFace.so");
			system("mv -f /home/root/update_dir/smartlocker/*   /opt/smartlocker/");
			system("mv -f /opt/smartlocker/libReadFace.so   /usr/lib/");

			SetSysUpdateFlagToCfgFile(0);
			RspStr[3] = 0xFF;  //update failed
			printf("=========System app and lib upgrade OK!===========\n");
		}
		else
		{
			RspStr[3] = 0x00;  //update failed
			printf("=========System app and lib upgrade Failed!===========\n");
		}
		
		/*֪ͨ�ⲿMCU�������*/
		len = sizeof(RspStr);
		RspStr[2] = (unsigned char)CMD_SYS_APP_UPDATE; //cmd id
		RspStr[3] = 0xFF;  //update success
		SendMsgToRpMSG(RspStr, (unsigned char)len);
		
		/*�ָ��ֳ���ɾ����ʱ�ļ�*/
		system("rm -rf /home/root/*");
		/*ж��U-disk*/
		system("umount /run/media/sda1");	
	}
	else
	{
		printf("Not exist update file in udisk, please check!\n");
	}
	
}

int main(int argc, char **argv)
{
	int ret=-1, rpmsg_fd=-1, val=0;
	unsigned char rpmsg_buf[4] = {0};	
	int faceId, len; 
	unsigned char RspStr[4] = {0x41, 0x37, 0x03, 0x00};  //'A' '7' cmd faceId	
	pid_t status;
	log_info("face_loop starting...\n");

	//tell MCU that A7 has Initialized OK.
	len = sizeof(RspStr);
	RspStr[2] = (unsigned char)RPMSG_A7_INIT_OK; //cmd id
	RspStr[3] = (unsigned char)0x01; //version
	SendMsgToRpMSG(RspStr, (unsigned char)len);

	//��������ͷģ���ϵ�LED
	SetOv7956ModuleLed(1);
	
	rpmsg_fd = open(RPMSG_DEV, O_RDWR);
	if(!rpmsg_fd)
	{
		printf("ttyRPMSG device open failed!\n");
	}
	
#ifdef APP_UPGRADE_USB
	/*����⵽����u�̣��������ļ��������Զ�U-disk�������������������*/
	if(CheckIsUDiskIn())
	{	
	 	stSysCfg stSystemCfg;
		GetSysLocalCfg(&stSystemCfg);
		if (stSystemCfg.auto_udisk_upgrade)
		{
			pthread_t threadPid;
		
			// create thread for system upgrade
			pthread_create(&threadPid, 0, SystemAppUpgrade, NULL);
		}
	}

	/*���������ɹ�����ļ����ж��Ƿ���Ҫ�ָ�����*/
	int Val = 0;
	GetSysUpdateFlagFromCfgFile(&Val);
	if(Val)
	{
		printf("=======start restore======\n");
		/*���������ɹ����,�ָ����ݵ�Ӧ�ú��㷨�⵽��ӦĿ¼*/
		system("rm -rf /opt/smartlocker/*");
		system("tar -xvf /tmp/face_app_bak.tar.gz   -C /opt/smartlocker/");
		system("cp -f /tmp/libReadFace.so	 /usr/lib/");
		SetSysUpdateFlagToCfgFile(0);
		printf("========restore ok=======\n");
	}
	/*upgrade���ܴ��ڼ�С���ܻ����face_loop���������г������⣬�����޷�������������rcS�м���
	��face_loop��̽�⣬���̽����ļ�ʧ�ܣ����԰ѱ�����/tmp/�Ļָ�����������������һʧ��*/
#endif	

	while(1)
	{
		val = 0;
		ret = ioctl(rpmsg_fd, RPMSG_READ, &val);  //transfer val addr to kernel
		if(ret == 0)
		{		
			memcpy(rpmsg_buf, &val, 4); 				
		}
		else
		{
			printf("rpmsg ioctl error: %d!\n", ret);
			break;
		}		

		if((rpmsg_buf[0] == 0x4d) && (rpmsg_buf[1] == 0x34))
		{
			printf("rpmsg_buf(user): cmd<0x%02x>,val<0x%02x>\n",  rpmsg_buf[2], rpmsg_buf[3]);		
			switch(rpmsg_buf[2])
			{
				case CMD_FACE_RECOGNITION:
					cout << "Switch to face recognize..." << endl;
					system(MAIN_WORK_PATH(tinylocker.sh));
					break;
					
				case CMD_FACE_REGISTRATION:
					cout << "Switch to face register..." << endl;	
					system(MAIN_WORK_PATH(facereg.sh));
					break;
					
				case CMD_FACE_DELETION:	
				{
					faceId = (int)rpmsg_buf[3];
					if(faceId < 0xFF)  // < 0x100 for kds
					{
						cout << "Delete specific face ID ..." << endl;						
						ret = faceDeleteProc(faceId);
						if(ret == RS_RET_OK)
							RspStr[3] = (unsigned char)faceId;
						else
						{
							RspStr[3] = 0xFF;  //delete ID fail
							cout << "Face ID delete fail!" << endl;
						}
					}
					else if(faceId == 0xFF)  //delete the database
					{	
						cout << "Delete the database file!!!" << endl;
						status = system("rm /opt/smartlocker/readsense.db");
						if(0 == WEXITSTATUS(status))
							RspStr[3] = 0x00;  //cmd run success
						else
						{
							RspStr[3] = 0xFF;  //delete fail
							cout << "Face database delete fail!" << endl;
						}
					}
					system("sync");

					len = sizeof(RspStr);
					RspStr[2] = (unsigned char)CMD_FACE_DELETION; //cmd id
					RspStr[3] = 0x00;  //start update mode
					SendMsgToRpMSG(RspStr, (unsigned char)len);
					
					break;
				}
				/*ϵͳAPP & �㷨������*/
				case CMD_SYS_APP_UPDATE:		
				{
					/*֪ͨ�ⲿMCU������ʼ����*/
					len = sizeof(RspStr);
					RspStr[2] = (unsigned char)CMD_SYS_APP_UPDATE; //cmd id
#ifdef APP_UPGRADE_USB
					RspStr[3] = 0x01;  //start update
					SendMsgToRpMSG(RspStr, (unsigned char)len);					
					SystemAppUpgrade(NULL);
#else
					RspStr[3] = 0x00;  //no support update
					SendMsgToRpMSG(RspStr, (unsigned char)len); 				
#endif
					break;
				}
				/*M4->A7 A7����VLLSģʽ*/
				case CMD_SYS_A7VLLS:		
				case CMD_SYS_A7SHUTDOWN:		
				{	
					//�ر�����ͷģ���ϵ�LED
					SetOv7956ModuleLed(0);
						
					/*A7����SHUTDOWNģʽǰ����ҪA7�Ƚ���VLLSģʽ*/
					/*֪ͨM4���ⲿMCU*/
					len = sizeof(RspStr);
					RspStr[2] = (unsigned char)rpmsg_buf[2]; //cmd id
					RspStr[3] = 0x01;  //A7 enter vlls mode
					SendMsgToRpMSG(RspStr, (unsigned char)len);

					system("echo \"Face Deletion done, A7 enter VLLS!\""); 
					system("echo mem > /sys/power/state");
					break;
				}
				
				default:
					cout << "No effective cmd from M4!" << endl;
					break;				
			}
		}
			
		usleep(50000);  //sleep 50ms
	}	

	close(rpmsg_fd);

	return 0;
}

