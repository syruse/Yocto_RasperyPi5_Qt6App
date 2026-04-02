FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += "file://wpa_supplicant.conf-sane"

# autolaunch for interface wlan0
SYSTEMD_SERVICE:${PN}:append = " wpa_supplicant@wlan0.service"
SYSTEMD_AUTO_ENABLE = "enable"

# force it to compile libwpa_client.so
do_compile:append() {
    oe_runmake -C wpa_supplicant libwpa_client.so
}

do_install:append() {
    #create directory
    install -d ${D}${sysconfdir}/wpa_supplicant
    #this is the path for "systemctl restart wpa_supplicant@wlan0"
    install -m 0600 ${UNPACKDIR}/wpa_supplicant.conf-sane ${D}${sysconfdir}/wpa_supplicant/wpa_supplicant-wlan0.conf
    install -m 0600 ${UNPACKDIR}/wpa_supplicant.conf-sane ${D}${sysconfdir}/wpa_supplicant.conf

    # install libwpa_client.so
    install -d ${D}${libdir}
    install -m 0755 ${S}/wpa_supplicant/libwpa_client.so ${D}${libdir}
    install -m 0644 ${S}/src/common/wpa_ctrl.h ${D}${includedir}
}

# add libwpa_client.so into package
FILES:${PN} += "${libdir}/libwpa_client.so"
PROVIDES += "libwpa_client"
