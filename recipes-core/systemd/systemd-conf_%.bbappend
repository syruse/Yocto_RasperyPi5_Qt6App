#for bbapend it is looking files nearby original bb file
FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += "file://25-wlan.network"

do_install:append() {
    install -d ${D}${sysconfdir}/systemd/network
    install -m 0644 ${UNPACKDIR}/25-wlan.network ${D}${sysconfdir}/systemd/network/
}

FILES:${PN} += "${sysconfdir}/systemd/network/25-wlan.network"
