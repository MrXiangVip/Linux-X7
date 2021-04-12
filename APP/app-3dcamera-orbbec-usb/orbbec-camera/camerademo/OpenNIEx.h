//
// Created by xlj on 17-3-6.
//

#ifndef OBDEPTH2_OBCAMEX_H
#define OBDEPTH2_OBCAMEX_H

#include <stdio.h>
//#include <android/log.h>
#include <OpenNI.h>
#include <map>
#include <memory>
#include <vector>
#include <iostream>

#define LOG_TAG "OpenNI-Jni"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG, __VA_ARGS__)

#define READ_WAIT_TIMEOUT 2000 //2000ms
#define HISTSIZE 0xFFFF

using namespace openni;
using namespace std;
typedef struct{
	int x;
	int y;
	int fps;
	PixelFormat format;
}Video_Mode;

class OpenNIEx{
public:
    OpenNIEx();
    ~OpenNIEx();
    int init();
	int enumerateDevices(vector<string>& uris);
    int open(string uri);
	int createStream(SensorType type);
	int readStream(SensorType type, VideoFrameRef& frame);
    int waitAnyUpdate();
    void close();
	int enable(SensorType type, bool flag);
	int setMode(SensorType type, int w, int h, int fps, PixelFormat format);
	int getMode(SensorType type,  vector<shared_ptr<Video_Mode>> vec);
	int IRToRGBA(VideoFrameRef& frame, uint8_t * texture, int textureWidth, int textureHeight);
    //int ConventToRGBA(uint8_t * dst, int w, int h);

   string getFormatName(openni::PixelFormat format);

   void* getFormatData(VideoFrameRef& frame, int type);
   int calNormalMap(VideoFrameRef& depth, void* normal);
   int ConventToRGBA(VideoFrameRef& depth, uint8_t * dst, int w, int h);
    
private:

    void calcDepthHist(VideoFrameRef& frame);
    VideoStream mColor;
    VideoStream mDepth;
    VideoStream mIR;

    VideoFrameRef mFrame;
    Device mDevice;
    uint32_t* m_histogram;
    int mWidth;
    int mHeight;

	shared_ptr<vector<string>> mUri;

	map<SensorType, shared_ptr<VideoStream>> m_VideoStreams;

	void *m_depthData;
};

#endif //OBDEPTH2_OBCAMEX_H
