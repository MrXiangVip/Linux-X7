//
// Created by xlj on 17-3-6.
//


//#include "uvc_protocol.h"
#include "OpenNIEx.h"
#include <math.h>


using namespace openni;

OpenNIEx::OpenNIEx(){

	mUri = make_shared<vector<string>>();
	m_depthData = NULL;
    m_histogram = new uint32_t[HISTSIZE];
    mWidth = 640;
    mHeight = 480;
}

OpenNIEx::~OpenNIEx(){

    delete m_histogram;
}

int OpenNIEx::init(){

    Status rc = OpenNI::initialize();

    if (rc != STATUS_OK)
    {
		std::cout << "Initialize failed: "<< OpenNI::getExtendedError()<< std::endl; 
        return -1;
    }

    return 0;
}

int OpenNIEx::enable(SensorType type, bool flag){

	auto stream = m_VideoStreams[type];
	if(stream == NULL){
       return -1;	
	}

	if(flag){
       stream->start();
	}else{
       stream->stop();
	}

	return 0;
}

int OpenNIEx::setMode(SensorType type, int w, int h, int fps, PixelFormat format){
  
    auto stream = m_VideoStreams[type];
      if(stream == NULL){
         std::cout << "set mode failed,  stream ("<< type<< ") is null"<< std::endl;
         return -1;
      }

	  VideoMode mode = stream->getVideoMode();
	  mode.setResolution(w, h);
	  mode.setFps(fps);
	  mode.setPixelFormat(format);
  
	  if(stream->setVideoMode(mode) != openni::STATUS_OK){
        m_VideoStreams.erase(type);
	    return -1;
	  }

	  return 0;
}


int OpenNIEx::enumerateDevices(vector<string>& uris){

    if(init() != STATUS_OK){
        return -1;
    }

    Array<DeviceInfo>  array;
    OpenNI::enumerateDevices(&array);
    if(array.getSize() <= 0){
		std::cout << "findn't  OpenNI  device"<< std::endl;
        return -1;
    }

    int size = array.getSize();
	if(size <= 0){
      return -1;	
	}

    for (int i = 0; i < size; ++i) {
        string  uri = array[i].getUri();
		//shared_ptr<string> uri = make_shared<string>();
		mUri->push_back(uri);
		uris.push_back(uri);
    }

    return 0;
}

string tochar(SensorType type){
	string name = "unknown";

	switch(type){
		case SENSOR_COLOR:
			name = "color";
			break;
		case SENSOR_DEPTH:
			name = "depth";
			break;
		case SENSOR_IR:
			name = "ir";
			break;
	}

	return name;
}

int OpenNIEx::createStream(SensorType type){

    Status rc;
    if (mDevice.getSensorInfo(type) == NULL)
    {
		std::cout << "Get "<< tochar(type)<<" Sensor failed " << __FILE__ << std::endl;
        return -1;
    } 
	auto s = m_VideoStreams[type];
	if(s == NULL){
		shared_ptr<VideoStream> stream = make_shared<VideoStream>();
		rc = stream->create(mDevice, type);
		if (rc != STATUS_OK)
		{
			std::cout << __FILE__ << "Couldn't create "<< tochar(type) <<" stream \n"<< OpenNI::getExtendedError()<< std::endl;
			return -1;
		}
		m_VideoStreams[type] = stream;
	}

	return 0;
}
 
int OpenNIEx::getMode(SensorType type,  vector<shared_ptr<Video_Mode>> vec)
{
	const SensorInfo* pSensorInfo = mDevice.getSensorInfo(type);
	const openni::Array<openni::VideoMode>& supportedModes = pSensorInfo->getSupportedVideoModes();
    for (int i = 0; i < supportedModes.getSize(); ++i)
    {
       const openni::VideoMode* pSupportedMode = &supportedModes[i];
       shared_ptr<Video_Mode> mode = make_shared<Video_Mode>();
	   mode->x = pSupportedMode->getResolutionX();
	   mode->y= pSupportedMode->getResolutionY();
	   mode->fps = pSupportedMode->getFps();
	   mode->format = pSupportedMode->getPixelFormat();
	   vec.push_back(mode);
   }
	

	return 0;
}

string OpenNIEx::getFormatName(openni::PixelFormat format){

string name = "";
 switch (format)
      {
      case openni::PIXEL_FORMAT_DEPTH_1_MM:
          name = "1 mm";
      case openni::PIXEL_FORMAT_DEPTH_100_UM:
          name = "100 um";
      case openni::PIXEL_FORMAT_SHIFT_9_2:
          name = "Shifts 9.2";
      case openni::PIXEL_FORMAT_SHIFT_9_3:
          name = "Shifts 9.3";
      case openni::PIXEL_FORMAT_RGB888:
          name = "RGB 888";
      case openni::PIXEL_FORMAT_YUV422:
          name = "YUV 422";
      case openni::PIXEL_FORMAT_YUYV:
          name = "YUYV";
      case openni::PIXEL_FORMAT_GRAY8:
          name = "Grayscale 8-bit";
      case openni::PIXEL_FORMAT_GRAY16:
          return "Grayscale 16-bit";
      case openni::PIXEL_FORMAT_JPEG:
          name = "JPEG";
      default:
          name = "Unknown";
      }

 return name;
}

int OpenNIEx::waitAnyUpdate(){
	int size = m_VideoStreams.size();
    VideoStream* pStream[size];
	int index = 0;
	for(auto it : m_VideoStreams ){
	   auto s = it.second;
	   pStream[index] = s.get();
       index++;
	}
    int changedStreamDummy;
    Status rc = OpenNI::waitForAnyStream(pStream, size, &changedStreamDummy, READ_WAIT_TIMEOUT);
    if (rc != STATUS_OK)
    {
		std::cout << "Wait failed! (timeout is " << READ_WAIT_TIMEOUT << " ms) :" << OpenNI::getExtendedError() << std::endl;
        return -1;
    }

    return 0;
}

int OpenNIEx::readStream(SensorType type, VideoFrameRef& frame){
   auto stream = m_VideoStreams[type];
   if(stream == NULL){
	   return -1;
   }
   Status rc = stream->readFrame(&frame);
    if (rc != STATUS_OK)
    {
		std::cout << "Read failed! " << OpenNI::getExtendedError() << std::endl;
        return -1;
    }
   return 0;
}

int OpenNIEx::IRToRGBA(VideoFrameRef& frame, uint8_t * texture, int textureWidth, int textureHeight){

    const uint8_t* pFrameData = (const uint8_t*)frame.getData();
    for (int y = 0; y < frame.getHeight(); ++y)
    {
        uint8_t* pTexture = texture + ((frame.getCropOriginY() + y) * textureWidth + frame.getCropOriginX()) * 4;
        const RGB888Pixel* pData = (const RGB888Pixel*)(pFrameData + y * frame.getStrideInBytes());
        for (int x = 0; x < frame.getWidth(); ++x, ++pData, pTexture += 4)
        {
            pTexture[0] = pData->r;
            pTexture[1] = pData->g;
            pTexture[2] = pData->b;
            pTexture[3] = 255;
        }
    }


    return 0;
}

int OpenNIEx::open(string uri){


    if(mDevice.open(uri.c_str()) != STATUS_OK){
		std::cout << "open "<< uri << " failed. "<< std::endl;
        return -1;
    }

    return 0;
}

void OpenNIEx::close(){


	for(auto it : m_VideoStreams){
	    auto stream = it.second;
		stream->stop();
		stream->destroy();
	}

	m_VideoStreams.clear();
    mDevice.close();
    OpenNI::shutdown();

    return ;
}

#if 1
int OpenNIEx::calNormalMap(VideoFrameRef& depth, void* normal)
{
	uint16_t* pFrameData = (uint16_t*)depth.getData();
	int width = depth.getWidth();
	int height = depth.getHeight();

	typedef uint16_t (*parray)[width];
    parray data = (parray) pFrameData;
    for (int y = 1; y < height - 1; y++)
    {
		int offset = y * width;
		float* pixel = (float*)normal+offset;
        for (int x = 1; x < width - 1; x++)
        {
            float  n[3];
            float viewport[3] = { 0.0, 0.0, 1.0 };
			if((x - 1 >= 0)&& (y - 1 >= 0)){
				n[0] = (float)data[y][x - 1] - (float)data[y][x];
				n[1] = (float)data[y - 1][x] - (float)data[y][x];
				n[2] = 1.0f;
				float p = 0.;
				for (int i = 0; i < 3; ++i)
				{
					p += n[i] * viewport[i];
				}
				if (p > 0)
				{
					n[0] = -n[0]; n[1] = -n[1]; n[2] = -n[2];
				}
				//normalize
				float norm = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
				float n1 = n[0] / norm; 
				float n2 = n[1] / norm; 
				float n3= n[2] / norm;
				*pixel++ = (n1 + 0.5);
				*pixel++ = (n2 + 0.5);
				*pixel++ = fabs(n3);

			}
        }
    }

    return 0;
}
#else
int OpenNIEx::calNormalMap(VideoFrameRef& depth, void* normal)
{
	if(depth.getFrameIndex() == 0 || !depth.isValid()){
	  return -1;
	}
	const openni::DepthPixel* pDepth = (openni::DepthPixel*)depth.getData();

	int width = depth.getWidth();
	int height = depth.getHeight();
    int originX = depth.getCropOriginX();
    int originY = depth.getCropOriginY();

	// copy depth into texture-map
		for (uint16_t nY = originY; nY < height + originY; nY++)
		{
			uint8_t* pTexture = TextureMapGetLine(&g_texDepth, nY) + originX*4;
			for (uint16_t nX = 0; nX < width; nX++, pDepth++, pTexture+=4)
			{
				uint8_t nRed = 0;
				uint8_t nGreen = 0;
				uint8_t nBlue = 0;
				uint8_t nAlpha = g_DrawConfig.Streams.Depth.fTransparency*255;

				XnUInt16 nColIndex;

				switch (g_DrawConfig.Streams.Depth.Coloring)
				{
				case LINEAR_HISTOGRAM:
					nRed = nGreen = g_pDepthHist[*pDepth]*255;
					break;
				case PSYCHEDELIC_SHADES:
					nAlpha *= (((XnFloat)(*pDepth % 10) / 20) + 0.5);
				case PSYCHEDELIC:
					switch ((*pDepth/10) % 10)
					{
					case 0:
						nRed = 255;
						break;
					case 1:
						nGreen = 255;
						break;
					case 2:
						nBlue = 255;
						break;
					case 3:
						nRed = 255;
						nGreen = 255;
						break;
					case 4:
						nGreen = 255;
						nBlue = 255;
						break;
					case 5:
						nRed = 255;
						nBlue = 255;
						break;
					case 6:
						nRed = 255;
						nGreen = 255;
						nBlue = 255;
						break;
					case 7:
						nRed = 127;
						nBlue = 255;
						break;
					case 8:
						nRed = 255;
						nBlue = 127;
						break;
					case 9:
						nRed = 127;
						nGreen = 255;
						break;
					}
					break;
				case RAINBOW:
					nColIndex = (XnUInt16)((*pDepth / (g_fMaxDepth / 256)));
					nRed   = PalletIntsR[nColIndex];
					nGreen = PalletIntsG[nColIndex];
					nBlue  = PalletIntsB[nColIndex];
					break;
				case CYCLIC_RAINBOW:
					nColIndex = (*pDepth % 256);
					nRed   = PalletIntsR[nColIndex];
					nGreen = PalletIntsG[nColIndex];
					nBlue  = PalletIntsB[nColIndex];
					break;
				case CYCLIC_RAINBOW_HISTOGRAM:
				{
					float fHist = g_pDepthHist[*pDepth];
					nColIndex = (*pDepth % 256);
					nRed   = PalletIntsR[nColIndex] * fHist;
					nGreen = PalletIntsG[nColIndex] * fHist;
					nBlue  = PalletIntsB[nColIndex] * fHist;
					break;
				}
				default:
					assert(0);
					return;
				}

				pTexture[0] = nRed;
				pTexture[1] = nGreen;
				pTexture[2] = nBlue;

				if (*pDepth == 0)
					pTexture[3] = 0;
				else
					pTexture[3] = nAlpha;
			}
		}

    return 0;
}
#endif

void* OpenNIEx::getFormatData(VideoFrameRef& frame, int type)
{
	if(m_depthData == NULL){
		//int size = sizeof(RGB888Pixel) * frame.getWidth() * frame.getHeight();
        int size = sizeof(uint32_t) * frame.getWidth() * frame.getHeight();
        m_depthData = new char[size];
	    memset(m_depthData, 0, size);
	}

    calNormalMap(frame, m_depthData);

  return m_depthData;
}



int OpenNIEx::ConventToRGBA(VideoFrameRef& frame, uint8_t * texture, int textureWidth, int textureHeight){



    const uint8_t* pFrameData = (const uint8_t*)frame.getData();
    calcDepthHist(frame);

    for (int y = 0; y < frame.getHeight(); ++y)
    {
        uint8_t* pTexture = texture + ((frame.getCropOriginY() + y) * textureWidth + frame.getCropOriginX()) * 3;
        const DepthPixel* pDepth = (const DepthPixel*)(pFrameData + y * frame.getStrideInBytes());
        for (int x = 0; x < frame.getWidth(); ++x, ++pDepth, pTexture += 3)
        {
            int val = m_histogram[*pDepth];
            pTexture[0] = val;
            pTexture[1] = val;
            pTexture[2] = 0x0;
        }
    }


    return 0;
}

void OpenNIEx::calcDepthHist(VideoFrameRef& frame)
{
    unsigned int value = 0;
    unsigned int index = 0;
    unsigned int x = 0;
    unsigned int y = 0;
    unsigned int numberOfPoints = 0;
    const DepthPixel* pDepth = (const DepthPixel*)frame.getData();

    if (m_histogram == NULL)
    {
        m_histogram = (uint32_t *)malloc(HISTSIZE * sizeof(int));
    }

    // Calculate the accumulative histogram
    memset(m_histogram, 0, HISTSIZE*sizeof(int));

	int depthSize = frame.getDataSize() / sizeof(DepthPixel);
    for (int i = 0; i < depthSize; ++i, ++pDepth)
    {
        value = *pDepth;

        if (value != 0)
        {
            m_histogram[value]++;
            numberOfPoints++;
        }
    }

    for (index = 1; index < HISTSIZE; index++)
    {
        m_histogram[index] += m_histogram[index - 1];
    }

    if (numberOfPoints != 0)
    {
        for (index = 1; index < HISTSIZE; index++)
        {
            m_histogram[index] = (unsigned int)(256 * (1.0f - ((float)m_histogram[index] / numberOfPoints)));
        }
    }
}
