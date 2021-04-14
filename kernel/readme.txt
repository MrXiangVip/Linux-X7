由于kernel里有同名文件，window下不支持，所以以压缩包的形式提交；
另外提交经常改的dts、config、driver


切换到0328 camera 的步骤：
1:make clean
2.修改：linux-kernel/arch/arm/configs/imx_7ulp_fastboot_defconfig  CONFIG_MXC_CAMERA_GC0328
3:使用：arch/arm/boot/dts/imx7ulp-lock-el2-fastboot-gc0328-ST7789.dtb
