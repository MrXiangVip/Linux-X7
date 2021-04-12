
#define OBC_API_EXPORT __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C"
{
#endif

  OBC_API_EXPORT int orbbec_Init(const char *config);

  /**
 * Set Depth Image Resolution, accuracy args.
 * @param resolutionX  Depth Image width
 * @param resolutionY  Depth Image height
 * @param zFactor     1: 1mm,   100: 0.1mm
 * @return  0, success < 0 failed
 */
  OBC_API_EXPORT int orbbec_RefreshWorldConversionCache(int resolutionX, int resolutionY, int zFactor);

  /**
 *  Converts a single point from the Depth coordinate system to the World coordinate system.
 * @param [in]   depthX   The X coordinate of the point to be converted, measured in pixels with 0.0 at the far left of the Depth image
 * @param [in]   depthY   The Y coordinate of the point to be converted, measured in pixels with 0.0 at the top of the Depth image
 * @param [in]   depthZ   Z(depth) coordinate of the point to be converted. Depth values of 16 bits
 * @param [out]  pWorldX  Pointer to a place to store the X coordinate of the output value, measured in millimeters in World coordinates
 * @param [out]  pWorldY  Pointer to a place to store the Y coordinate of the output value, measured in pixels with 0 at top of image
 * @param [out]  pWorldZ  pWorldZ Pointer to a place to store the Z coordinate of the output value,
 * @return 0 success, < 0 failed
 */
  OBC_API_EXPORT int
  orbbec_ConvertDepthToWorldCoordinates(float depthX, float depthY, float depthZ, float *pWorldX,
                                        float *pWorldY, float *pWorldZ);

  /**
 * Converts a single point from the World coordinate system to the Depth coordinate system.
 * @param [in] worldX The X coordinate of the point to be converted, measured in millimeters in World coordinates
 * @param [in] worldY The Y coordinate of the point to be converted, measured in millimeters in World coordinates
 * @param [in] worldZ The Z coordinate of the point to be converted, measured in millimeters in World coordinates
 * @param [out] pDepthX Pointer to a place to store the X coordinate of the output value, measured in pixels with 0 at far left of image
 * @param [out] pDepthY Pointer to a place to store the Y coordinate of the output value, measured in pixels with 0 at top of image
 * @param [out] pDepthZ Pointer to a place to store the Z(depth) coordinate of the output value,
 * @return 0 success, < 0 failed
 */
  OBC_API_EXPORT int
  orbbec_ConvertWorldToDepthCoordinates(float worldX, float worldY, float worldZ, float *pDepthX,
                                        float *pDepthY, float *pDepthZ);

  /**
 * UnPack 10bit Raw data.
 * @param [in] src 10bit raw data
 * @param [out] dst 16bit unpack data, High six place fill 0
 * @param [in] w   width
 * @param [in] h   height
 * @return 0: success, < 0 failed
 */
  OBC_API_EXPORT int orbbec_UnPackRaw10(uint8_t *src, uint16_t *dst, int w, int h);

  /**
 * UnPack 12bit Raw data.
 * @param [in] src 12bit raw data
 * @param [out] dst 16bit unpack data, High four place fill 0
 * @param [in] w   width
 * @param [in] h   height
 * @return 0: success, < 0 failed
 */
  OBC_API_EXPORT int orbbec_UnPackRaw12(uint8_t *src, uint16_t *dst, int w, int h);

  /**
 * convert 16bit data to depth data
 * @param [in] src 16bit unpack raw data
 * @param [out] dst 16bit depth data
 * @param [in] w   width
 * @param [in] h   height
 * @return 0: success, < 0 failed
 */
  OBC_API_EXPORT int orbbec_Shift_2_Depth(uint16_t *src, uint16_t *dst, uint16_t w, uint16_t h);

  /**
 * convert 10bit data to depth data
 * @param [in] src 10bit raw data
 * @param [out] dst 16bit unpack data, High four place fill 0
 * @param w  width
 * @param h  height
 * @return 0: success, < 0 failed
 */
  OBC_API_EXPORT int orbbec_Raw10_2_Depth(uint8_t *src, uint16_t *dst, int w, int h);

  /**
 * convert 12bit raw data to depth data,This operation is the same as  call function UnPackRaw12 and shift9_2ToDepth
 * @param  [in] src 12bit raw data
 * @param  [out] dst 16bit depth data
 * @param  [in] w   width
 * @param  [in] h   height
 * @return 0: success, < 0 failed
 */
  OBC_API_EXPORT int orbbec_Raw12_2_Depth(uint8_t *src, uint16_t *dst, int w, int h);

  OBC_API_EXPORT int orbbec_Release();

  OBC_API_EXPORT int orbbec_GetVersion(char *buf, int size);

  OBC_API_EXPORT void orbbec_EnableDebug(bool enable);

  OBC_API_EXPORT int ConventFromDepthToRGBA(short *src, int *dst, int w, int h, int strideInBytes);

  /**
 * judge the frame is depth or ir.
 * @param  [pInput] src 12bit raw data
 * @return 1 : DE_DEPTH, 2 : MODE_IR
 */
  OBC_API_EXPORT int orbbec_parseFrame(unsigned short *pInput);

  OBC_API_EXPORT int Ir_2_RGB888(unsigned short *pInput, unsigned int nInputSize, unsigned char *pOutput);

  OBC_API_EXPORT int Raw_Shift_12bit(unsigned short *pInput, unsigned int nInputSize, unsigned char *pOutput);

  OBC_API_EXPORT int Raw_Shift_10bit(unsigned short *pInput, unsigned int nInputSize, unsigned char *pOutput);

  OBC_API_EXPORT void Raw_Shift_Right_4bit(unsigned short *pInput, unsigned int nInputSize, unsigned char *pOutput);

#ifdef __cplusplus
}
#endif
