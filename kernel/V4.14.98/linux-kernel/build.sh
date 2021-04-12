echo "make.."
make ARCH=arm imx_7ulp_fastboot_defconfig
make ARCH=arm v=j8
echo "cp image dtb.."
cp ./arch/arm/boot/zImage ./
cp ./arch/arm/boot/dts/imx7ulp-lock-el2-fastboot-ov7725-ST7789.dtb   ./
mv ./imx7ulp-lock-el2-fastboot-ov7725-ST7789.dtb   ./zImage-imx7ulp-lock-emmc.dtb 
