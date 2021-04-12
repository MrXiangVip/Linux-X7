/**************************************************************************
 * 	FileName:	 sys_upgrade.cpp
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

#include "function.h"
#include "sys_upgrade.h"


//判断是否插入U盘
BOOL CheckIsUDiskIn()
{
	char buf[512] = {0};
	FILE *fp = NULL;
	char* cmd1 = "ls /sys/class/scsi_device/";
	
	fp = popen( cmd1 , "r");
	fread(buf, 1, 7, fp);		
	//printf("USB drive buf now. buf = %s  \n",buf);
	pclose(fp);
		
	// 未查出U盘信息，判空
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
		
		/* 将文件同步,及时写入磁盘,不然，系统重启动，导致文件内容丢失 */
		fflush(pFile);		/* 刷新内存缓冲，将内容写入内核缓冲 */
		fsync(fileno(pFile));	/* 将内核缓冲写入磁盘 */
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
		/*如果不存在配置文件，那么用系统默认的值，并生成配置文件*/
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

			/*解压*/
			system("mkdir /home/root/update_dir");
			system("tar -xvf /home/root/Face_Update.bin -C /home/root/update_dir");
			
			/*预留一个可升级的升级时执行的脚步，可以在里面做一些临时的除算法和应用目录外的升级*/
			system("/opt/smartlocker/upgrade.sh");
			sleep(1);
			
			/*备份即将的升级目录*/
			system("tar -cvzf /tmp/face_app_bak.tar.gz  /opt/smartlocker/");
			system("cp /usr/lib/libReadFace.so   /tmp/");
			
			/*写升级标记到文件，当替换过程中断电等异常时，没有升级成功，就把上面备份的恢复*/
			SetSysUpdateFlagToCfgFile(1);

			/*替换旧的应用和算法库*/
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
		
		/*通知外部MCU升级结果*/
		//cmdSysUpdateRsp(0xFF);
		
		/*恢复现场，删除临时文件*/
		system("rm -rf /home/root/*");
		/*卸载U-disk*/
		system("umount /run/media/sda1");	
	}
	else
	{
		printf("Not exist update file in udisk, please check!\n");
	}
	
}

void SysUpdateTaskProc()
{
	/*当检测到挂载u盘，且配置文件配置了自动U-disk升级，则进入升级流程*/
	if(CheckIsUDiskIn())
	{	
		pthread_t threadPid;
	
		// create thread for system upgrade
		pthread_create(&threadPid, 0, SystemAppUpgrade, NULL);
	}

	/*根据升级成功标记文件来判断是否需要恢复数据*/
	int Val = 0;
	GetSysUpdateFlagFromCfgFile(&Val);
	if(Val)
	{
		printf("=======start restore======\n");
		/*有升级不成功标记,恢复备份的应用和算法库到相应目录*/
		system("rm -rf /opt/smartlocker/*");
		system("tar -xvf /tmp/face_app_bak.tar.gz	-C /opt/smartlocker/");
		system("cp -f /tmp/libReadFace.so	 /usr/lib/");
		SetSysUpdateFlagToCfgFile(0);
		printf("========restore ok=======\n");
	}
	/*upgrade功能存在极小可能会出现face_loop升级过程中出现问题，导致无法启动，可以在rcS中加入
	对face_loop的探测，如果探测该文件失败，可以把备份在/tmp/的恢复回来，这样就万无一失了*/
}

