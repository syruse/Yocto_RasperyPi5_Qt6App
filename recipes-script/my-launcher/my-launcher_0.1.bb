SUMMARY = "My launcher script"
SECTION = "my scripts"
LICENSE = "CLOSED"

SRC_URI = "file://my-launcher.sh \
           file://my-launcher.service \
           "

S = "${UNPACKDIR}"

inherit systemd

SYSTEMD_SERVICE:${PN} = "my-launcher.service"

do_install() {
        install -d ${D}${bindir}
        install -m 0755 ${S}/my-launcher.sh ${D}${bindir}/my-launcher.sh
        install -d ${D}${systemd_unitdir}/system
        install -m 0644 ${S}/my-launcher.service ${D}${systemd_unitdir}/system/my-launcher.service
}

FILES:${PN} = "${bindir}/my-launcher.sh \
               ${systemd_unitdir}/system/my-launcher.service"
