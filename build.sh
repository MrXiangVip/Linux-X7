#!/bin/bash
display_help()
{
	echo "Usage:"
	echo "        build.sh       :编译app和kernel"
	echo "        build.sh  clean:清理app和kernel"
}

build_app()
{
  echo "编译APP"
  cd $VPATH
  cd $VPATH/APP/app-2dcamera-ov7725;
  make
  echo "拷贝APP"
  cp  face_loop   $OUT
  cp  face_register   $OUT
  cp  face_recognize   $OUT
  cp  mqtt          $OUT
}
clean_app()
{
  echo "清理APP"
  cd $VPATH
  cd $VPATH/APP/app-2dcamera-ov7725;
  make clean
}
build_kernel()
{
  echo "编译kernel"
  cd $VPATH
  cd $VPATH/kernel/V4.14.98/linux-kernel;
  pwd
  make  ARCH=arm imx_7ulp_fastboot_defconfig
  make  v=j8
  echo "拷贝zImage, dtb"
  cp  ./arch/arm/boot/zImage $OUT/
  cp  ./arch/arm/boot/dts/imx7ulp-lock-el2-fastboot-ov7725-ST7789.dtb $OUT/zImage-imx7ulp-lock-emmc.dtb
}
clean_kernel()
{
  echo "清理kernel"
  cd $VPATH
  cd $VPATH/kernel/V4.14.98/linux-kernel;
  pwd
  make clean
}
pack_version()
{
  echo "打包版本"
  cd $OUT
  pwd
  echo "打包文件系统"
  tar -xjf rootfs.tar.bz2 ./
  mv  face_loop       ./opt/smartlocker
  mv  face_register   ./opt/smartlocker
  mv  face_recognize  ./opt/smartlocker
  mv  mqtt            ./opt/smartlocker
  rm rootfs.tar.bz2
  tar -cjf   rootfs.tar.bz2  ./bin  ./boot ./dev ./etc ./home ./lib ./media ./mnt ./opt ./proc ./run ./sbin ./sys ./tmp ./usr ./var
  rm -rf ./bin  ./boot ./dev ./etc ./home ./lib ./media ./mnt ./opt ./proc ./run ./sbin ./sys ./tmp ./usr ./var

#  echo "打包压缩包"
#  cd $VPATH
#  if [ ! -f "out.zip" ];then
#    zip  -r out.zip out/
#  else
#    rm out.zip
#    zip  -r out.zip out/
#  fi
  echo $(date)"   DONE!"
}
clean_pack()
{
  echo "删除out.zip"
  cd $VPATH
  rm out.zip
}
do_main()
{
  echo $0 $1
  echo "初始化环境变量"
  source  /opt/fsl-imx-wayland/4.9.88-2.0.0/environment-setup-cortexa7hf-neon-poky-linux-gnueabi
  VPATH=$(pwd)
  echo $VPATH
  OUT=${VPATH}/out
  echo $OUT

	if [ $# -eq 0 ]; then
	  build_app
    build_kernel
    pack_version
		exit
	fi

  command=$1
	if [ "${command}" = "clean" ]; then
    clean_app
    clean_kernel
	else
    display_help
  fi
}

do_main ${1}







