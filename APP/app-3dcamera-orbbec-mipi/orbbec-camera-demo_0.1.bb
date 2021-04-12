SUMMARY = "APP-ORBBEC-CAMERA-DEMO"
DESCRIPTION = "This is a orbbec camera mipi2dvp Demo"
HOMEPAGE = "http://quitcoding.com/?page=work#cinex"

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

#inherit deploy

SRC_URI = " \
	file://camera-demo \
	"
SRCREV = "${AUTOREV}"

S = "${WORKDIR}/camera-demo"


do_compile() {
	cd ${S}
	make 
}

do_install() {
	install -d "${D}/opt/${PN}"
	install -d "${D}/${libdir}"
	install -d "${D}/${sysconfdir}"
	install -m 0755 ${B}/orbbec_rs_recg_face ${D}/opt/${PN}
	install -m 0755 ${B}/orbbec_rs_reg_face ${D}/opt/${PN}
	install -m 0755 ${B}/face_loop ${D}/opt/${PN}	
	install -m 0755 ${B}/face_loop.sh ${D}/opt/${PN}
	install -m 0755 ${B}/face_register.sh ${D}/opt/${PN}
	install -m 0755 ${B}/face_recg.sh ${D}/opt/${PN}
#	install -m 0755 ${B}/rcS ${D}${sysconfdir}/init.d/rcS
#	install -m 0755 ${B}/rcK ${D}${sysconfdir}/init.d/rcK
	install -m 0755 ${B}/libodepth/config1.ini ${D}${sysconfdir}/config1.ini
	cp ${S}/librs/libReadFace.so ${D}/${libdir}	
	cp ${S}/libyuv/libyuv.a ${D}/${libdir}
	cp ${S}/libodepth/libodepth.so ${D}/${libdir}
#	cp -R ${S}/syslib/* ${D}/${libdir}	
}

do_package_qa[noexec] = "1"
INSANE_SKIP_${PN}_append = "already-stripped"

FILES_${PN} += "opt/${PN}"

COMPATIBLE_MACHINE = "(mx7ulp)"
