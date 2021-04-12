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
#include <condition_variable>
#include <mutex>
#include <thread>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/videodev2.h>
#include <linux/input.h>

#include "RSApiHandle.h"

int main(int argc, char **argv)
{
	RSApiHandle rsHandle;
	cout << "RSAPI Test" << endl;
	if(!rsHandle.Init()) {
		cerr << "Failed to init the ReadSense lib" << endl;
		return -1;
	}

	return 0;
}
