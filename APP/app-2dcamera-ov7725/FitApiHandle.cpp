#include <iostream>
#include "FitApiHandle.h"
#include <sys/time.h>
#include <math.h>
#include "function.h"
#include <string.h>

#include "pfmgr.h"

#define PER_TIME_CAL  1
#define LOG_USE_COLOR
FIT g_face;    // ���ڱ���ע��ʱ��������ֵ

int FitlibInit()
{
#if PER_TIME_CAL
	per_calc_start();
#endif
	/** ��ʼ��ģ�� */
	int ret = g_face.Initial("");  // һ��ʵ��ֻ��ִ��һ�Σ����� 0 ��ʾ��ʼ���ɹ���ģ�ͳ�ʼ���������ӿڿ���������
	if (0 != ret) 
	{
		log_error("ERROR: model loading failed %d\n", ret);
		return -1;
	}
#if PER_TIME_CAL
	per_calc_end();
	log_set_color(1);
	log_info("FIT_Init OK!used<%d us>\n", per_calc_get_result());
#endif

	g_face.SetPara("recognizeThreshold", stSystemCfg.recognizeThreshold);//50
	g_face.SetPara("posFilterThreshold", stSystemCfg.posFilterThreshold);       ///1.0f
	g_face.SetPara("faceMinSize", stSystemCfg.faceMinSize);  //0
	g_face.SetPara("faceMaxSize", stSystemCfg.faceMaxSize);//400
	g_face.SetPara("faceMinLight", stSystemCfg.faceMinLight); //0
	g_face.SetPara("faceMaxLight", stSystemCfg.faceMaxLight);//180
	g_face.ClearRegisteredFace();

	log_debug("Fit lib init OK.\n");	
	return 0;
}

void GetFitVersion(char * pstrVer)
{
	g_face.GetVersion(pstrVer);
}

/** �������������������������ƶȣ����ƶȷ�����Χ [0, 100]������Խ��˵��Խ���� */
float FaceVerify( float* feature1,  float* feature2) 
{
  if (feature1 == NULL || feature2 == NULL) 
  {
    return 0.0;
  }
  float score = 0;
  for(int i = 0; i < 256; ++i) {
    score += feature1[i] * feature2[i];
  }
  return score * 50.f + 50.f;
}

int recognizeFace(const byte *imgBuf, uUID * pUU_ID, int *pPersonID) 
{
  int i = 0;
  int ret;
  int j = 0;

  // �������������������ȣ�һ����������������Ϊ 256 �����������ֽڳ���Ϊ 256*sizeof(float) �ֽڣ��� 2048 ���ֽ�
  const int featurelen = 256;
  // �������������������ȵĿռ�
  float features[featurelen * 2];


    // ��ȡͼ�������������������������
    int light = 0;
    EFaceState  faceState; //ֻ�м�⵽��������������
    SFaceFt *feature = NULL;  // ���������������
    SRect faceRect;

    /*���� FIT ExtractFeature �ӿڼ�����ȡͼƬ������������������������⵽�Ϸ���������
    �ڲ����������������ȡ�������� 0�����򷵻ط� 0 ֵ����ʾ��ȡʧ��*/
    // image->pData ͼ�����ذ� BGR ��ʽ��ŵ��ֽ���
    // image->width ͼ����
    // image->height ͼ��߶�
    // light �����������������ֵ
    // faceState ���������״̬��Ϣ
    // feature �������������
    // faceRect �������������Ϣ
	float score = 0.0f;

	ret = g_face.Recognize1_n(imgBuf, CAM_HEIGHT, CAM_WIDTH, NULL, p_person_face_mgr.getPersonFaces(), 
							p_person_face_mgr.m_person_faces.size(), light,faceState, score, &faceRect, true);
	if (ret < 0) 
	{
		log_info("feature recognize failed ret %d.\n", ret);

		return ret;
	}
	// ��ӡ��⵽��������Ϣ��light����ʾ������������ֵ��rect ��ʶ������λ����Ϣ
	log_debug("DETECTED INFO ret<%d>, light:%d,rect<%d, %d, %d, %d>,score<%f>,faceState:<%d>\n",
	ret, light, faceRect.x, faceRect.y, faceRect.height, faceRect.width, score, faceState);
	
	PERSONFACE_MAP person_ids = p_person_face_mgr.m_person_faces;
	PERSONFACE_MAP::iterator it; 
	int idx = 0;
	for(it = person_ids.begin(); it != person_ids.end(); ++it) 
	{
		if (idx++ == ret) 
		{
			memcpy(pUU_ID, &(it->second.uu_id), sizeof(uUID));
			*pPersonID = it->second.personId;
			//cout << "Recognized person_id: " << it->second.personId << endl;
		}
	}

	return ret;
}

int registerFace(const byte *imgBuf) 
{
  int i = 0;
  int ret;
  int j = 0;

  // �������������������ȣ�һ����������������Ϊ 256 �����������ֽڳ���Ϊ 256*sizeof(float) �ֽڣ��� 1024 ���ֽ�
  const int featurelen = 256;
  // �������������������ȵĿռ�
  float features[featurelen * 2];


    // ��ȡͼ�������������������������
    int light = 0;
    EFaceState  faceState;
    SFaceFt *feature = NULL;  // ���������������
    SRect faceRect;

    // ���� FIT ExtractFeature �ӿڼ�����ȡͼƬ������������������������⵽�Ϸ����������ڲ����������������ȡ�������� 0�����򷵻ط� 0 ֵ����ʾ��ȡʧ��
    // image->pData ͼ�����ذ� BGR ��ʽ��ŵ��ֽ���
    // image->width ͼ����
    // image->height ͼ��߶�
    // light �����������������ֵ
    // faceState ���������״̬��Ϣ
    // feature �������������
    // faceRect �������������Ϣ
	int num = 0;
      ret = g_face.Register(imgBuf,
      CAM_HEIGHT, CAM_WIDTH, NULL, light,
      faceState, num, &faceRect, true);
	if (0 != ret) 
	{
		log_debug("feature register failed ret %d.\n", ret);
	}

    	// ��ӡ��⵽��������Ϣ��light����ʾ������������ֵ��rect ��ʶ������λ����Ϣ
	log_debug("DETECTED INFO ret<%d>, light:%d,rect<%d, %d, %d, %d>,faceState:<%d>\n",
	ret, light, faceRect.x, faceRect.y, faceRect.height, faceRect.width, faceState);

	return ret;
}

int getSfaceBuffer(SFaceFt *sface, unsigned char *buffer, int &buf_len) 
{
	printf("sface len is %d\n", sizeof(SFaceFt));
	printf("sface buflen is %d\n", sface->nLocalFtLen);
	buf_len = sizeof(SFaceFt) + sface->nLocalFtLen;
	printf("buf len is %d\n", buf_len);
	buffer = new unsigned char[buf_len];
	memcpy(buffer, sface, sizeof(sface));
	memcpy(buffer + sizeof(SFaceFt), sface->pbLocalFt, sface->nLocalFtLen);
		for (int i = 0; i < buf_len ; i++) 
		{
			printf("%02d ", buffer[i]);
			if (i % 32 == 31)
				printf("\n");
		}
		printf("\n");
	return 0;
}

void saveFace() 
{
	SFaceFt* sface = g_face.GetFeature();
	if (sface != 0) 
	{
		printf("register succuss!\n");
		printf("sface len is %d\n", sizeof(SFaceFt));
		printf("sface buflen is %d\n", sface->nLocalFtLen);
		int buf_len = sizeof(SFaceFt) + sface->nLocalFtLen;
		printf("buf len is %d\n", buf_len);
		unsigned char *buf = new unsigned char[buf_len];
		memcpy(buf, sface, sizeof(SFaceFt));
		memcpy(buf + sizeof(SFaceFt), sface->pbLocalFt, sface->nLocalFtLen);
			// getSfaceBuffer(sface, buf, buf_len);
			printf("--------- buf_len is %d\n", buf_len);
			FILE *fp;     
			if (!(fp = fopen("my.feature", "wb")))  {
			printf("file open fail\n");
				return;     
			}
			printf("file open ok\n");
			for (int i = 0; i < buf_len ; i++) {
				printf("%02x ", buf[i]);
				if (i % 32 == 31)
					printf("\n");
			}
			printf("\n");
			fwrite(buf, sizeof(unsigned char), buf_len, fp);
			
			/* ���ļ�ͬ��,��ʱд�����,��Ȼ��ϵͳ�������������ļ����ݶ�ʧ */
			fflush(fp);		/* ˢ���ڴ滺�壬������д���ں˻��� */
			fsync(fileno(fp));	/* ���ں˻���д����� */
			fclose(fp);
			printf("save file ok\n");

	}
	else 
	{
		printf("register failed!\n");
	}
}


#define RGB565_MASK_RED 0xF800
#define RGB565_MASK_GREEN 0x07E0
#define RGB565_MASK_BLUE 0x001F
void RGB565ToRGB(unsigned short *pInput, unsigned int nInputSize, unsigned char *pRgb)
{
	unsigned short *pInputTmp = pInput;
	for (unsigned int i = 0; i < nInputSize; i++)
	{
		unsigned char *rgb24 = pRgb;

		rgb24[2] = (*pInputTmp & RGB565_MASK_RED) >> 11;
		rgb24[1] = (*pInputTmp & RGB565_MASK_GREEN) >> 5;
		rgb24[0] = (*pInputTmp & RGB565_MASK_BLUE);
		rgb24[2] <<= 3;
		rgb24[1] <<= 2;
		rgb24[0] <<= 3;

		pRgb += 3;
		pInputTmp++;
	}
}
void RGB565ToRGBA(unsigned short *pInput, unsigned int nInputSize, unsigned char *pRgba)
{
	unsigned short *pInputTmp = pInput;
	for (unsigned int i = 0; i < nInputSize; i++)
	{
		unsigned char *rgb32 = pRgba;

		rgb32[2] = (*pInputTmp & RGB565_MASK_RED) >> 11;
		rgb32[1] = (*pInputTmp & RGB565_MASK_GREEN) >> 5;
		rgb32[0] = (*pInputTmp & RGB565_MASK_BLUE);
		rgb32[2] <<= 3;
		rgb32[1] <<= 2;
		rgb32[0] <<= 3;
		rgb32[3] = 0xFF;

		pRgba += 4;
		pInputTmp++;
	}
}



 int RGB888ToRGB565(void * psrc, int w, int h, void * pdst)
{
    unsigned char * psrc_temp;
    unsigned short * pdst_temp;
 
    unsigned int i,j;
 
    if (!psrc || !pdst || w <= 0 || h <= 0) {
        printf("rgb888_to_rgb565 : parameter error\n");
        return -1;
    }
 
    psrc_temp = (unsigned char *)psrc;
    pdst_temp = (unsigned short *)pdst;
    for (i=0; i<h; i++) {
        for (j=0; j<w; j++) {
            //888 r|g|b -> 565 b|g|r
            *pdst_temp =(((psrc_temp[2] >> 3) & 0x1F) << 11)//b
                        |(((psrc_temp[1] >> 2) & 0x3F) << 5)//g
                        |(((psrc_temp[0] >> 3) & 0x1F) << 0);//r
 
            psrc_temp += 3;
            pdst_temp ++;
        }
       // psrc_temp +=(3*16); 
    }
 
    return 0;
}


void RotateRGB(const unsigned char* src,unsigned char* result,int width,int height,int mode)
{// mode 0 -> 0; 1 -> 90; 2->180; 3->270;
	if(mode == 1)
	{
		int x = 0;
		int y = 0;
		int posR = 0;
		int posS = 0;
		for(x = 0; x < width; x++)
		{
			for(y = height - 1; y >= 0; y--)
			{
				posS = (y * width + x) * 3;
				result[posR + 0] = src[posS + 0];// R
				result[posR + 1] = src[posS + 1];// G
				result[posR + 2] = src[posS + 2];// B
				// result[posR + 3] = src[posS + 3];// A
				posR += 3;
			}
		}
	}
}

// ��ת�ֽ�˳�� (16-bit)
uint16_t ReverseBytes(uint16_t value)
{
	return (uint16_t)((value & 0xFFU) << 8 | (value & 0xFF00U) >> 8);
}

void Rgb565BE2LE(const uint16_t* src_rgb, uint16_t* dst_rgb, int width) 
{
  for (int x = 0; x < width; x+=16) {
	//  printf("before: %d", src_rgb[0]);
	dst_rgb[0] = (uint16_t)((src_rgb[0] & 0xFFU) << 8 | (src_rgb[0] & 0xFF00U) >> 8);
	dst_rgb[1] = (uint16_t)((src_rgb[1] & 0xFFU) << 8 | (src_rgb[1] & 0xFF00U) >> 8);
	dst_rgb[2] = (uint16_t)((src_rgb[2] & 0xFFU) << 8 | (src_rgb[2] & 0xFF00U) >> 8);
	dst_rgb[3] = (uint16_t)((src_rgb[3] & 0xFFU) << 8 | (src_rgb[3] & 0xFF00U) >> 8);
	dst_rgb[4] = (uint16_t)((src_rgb[4] & 0xFFU) << 8 | (src_rgb[4] & 0xFF00U) >> 8);
	dst_rgb[5] = (uint16_t)((src_rgb[5] & 0xFFU) << 8 | (src_rgb[5] & 0xFF00U) >> 8);
	dst_rgb[6] = (uint16_t)((src_rgb[6] & 0xFFU) << 8 | (src_rgb[6] & 0xFF00U) >> 8);
	dst_rgb[7] = (uint16_t)((src_rgb[7] & 0xFFU) << 8 | (src_rgb[7] & 0xFF00U) >> 8);
	dst_rgb[8] = (uint16_t)((src_rgb[8] & 0xFFU) << 8 | (src_rgb[8] & 0xFF00U) >> 8);
	dst_rgb[9] = (uint16_t)((src_rgb[9] & 0xFFU) << 8 | (src_rgb[9] & 0xFF00U) >> 8);
	dst_rgb[10] = (uint16_t)((src_rgb[10] & 0xFFU) << 8 | (src_rgb[10] & 0xFF00U) >> 8);
	dst_rgb[11] = (uint16_t)((src_rgb[11] & 0xFFU) << 8 | (src_rgb[11] & 0xFF00U) >> 8);
	dst_rgb[12] = (uint16_t)((src_rgb[12] & 0xFFU) << 8 | (src_rgb[12] & 0xFF00U) >> 8);
	dst_rgb[13] = (uint16_t)((src_rgb[13] & 0xFFU) << 8 | (src_rgb[13] & 0xFF00U) >> 8);
	dst_rgb[14] = (uint16_t)((src_rgb[14] & 0xFFU) << 8 | (src_rgb[14] & 0xFF00U) >> 8);
	dst_rgb[15] = (uint16_t)((src_rgb[15] & 0xFFU) << 8 | (src_rgb[15] & 0xFF00U) >> 8);
	// dst_rgb[0] = ReverseBytes(src_rgb[0]);
	//  printf("\tafter: %d", dst_rgb[0]);
    dst_rgb+=16;
    src_rgb+=16;
  }
}

//����RGB image
void  ScaleRGB(void* pDest, int nDestWidth, int nDestHeight, int nDestBits, void* pSrc, int nSrcWidth, int nSrcHeight, int nSrcBits)
{
 //������Ч�Լ��
 //ASSERT_EXP(pDest != NULL);
 //ASSERT_EXP((nDestBits == 32) || (nDestBits == 24));
 //ASSERT_EXP((nDestWidth > 0) && (nDestHeight > 0));
 
 //ASSERT_EXP(pSrc != NULL);
 //ASSERT_EXP((nSrcBits == 32) || (nSrcBits == 24));
 //ASSERT_EXP((nSrcWidth > 0) && (nSrcHeight > 0));
 
 //��dfAmplificationX��dfAmplificationY�ֱ�洢ˮƽ�ʹ�ֱ����ķŴ���
 double dfAmplificationX = ((double)nDestWidth)/nSrcWidth;
 double dfAmplificationY = ((double)nDestHeight)/nSrcHeight;
 
 //���㵥��Դλͼ��ɫ��Ŀ��λͼ��ɫ��ռ�ֽ���
 const int nSrcColorLen = nSrcBits/8;
 const int nDestColorLen = nDestBits/8;
 
 //����ͼƬ���ż���
 for(int i = 0; i<nDestHeight; i++)      //�����i��
  for(int j = 0; j<nDestWidth; j++)   //�����i���е�j��
  {
   //------------------------------------------------------
   //���´��뽫����nLine��nRow��ֵ,����Ŀ�ľ����е�(i, j)��
   //ӳ��ΪԴ�����е�(nLine, nRow)��,����,nLine��ȡֵ��ΧΪ
   //[0, nSrcHeight-1],nRow��ȡֵ��ΧΪ[0, nSrcWidth-1], 
   
   double tmp = i/dfAmplificationY;
   int nLine = (int)tmp;
   
   if(tmp - nLine > 0.5)
    ++nLine;
   
   if(nLine >= nSrcHeight)
    --nLine;
   
   tmp = j/dfAmplificationX;
   int nRow = (int)tmp;
   
   if(tmp - nRow > 0.5)
    ++nRow; 
   
   if(nRow >= nSrcWidth)
    --nRow;   
   
   unsigned char *pSrcPos = (unsigned char*)pSrc + (nLine*nSrcWidth + nRow)*nSrcColorLen;
   unsigned char *pDestPos = (unsigned char*)pDest + (i*nDestWidth + j)*nDestColorLen;
   
   //��pSrcPosλ�õ�ǰ���ֽڿ�����pDestPos����
   *pDestPos++ = *pSrcPos++;
   *pDestPos++ = *pSrcPos++;
   *pDestPos++ = *pSrcPos++;
   
   if(nDestColorLen == 4)
    *pDestPos = 0;
  }
} 

