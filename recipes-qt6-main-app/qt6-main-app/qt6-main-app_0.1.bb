SUMMARY = "qt6 based main app"
LICENSE = "CLOSED"

DEPENDS += " cmake qtbase qtdeclarative qtdeclarative-native wpa-supplicant "
RDEPENDS:${PN} += " qtbase qtdeclarative wpa-supplicant"

# for virtual keyboard support
RDEPENDS:${PN} += " \
    qtvirtualkeyboard \
    qtbase-plugins \
    qtdeclarative-qmlplugins"

SRC_URI = "file://HMI "
S = "${UNPACKDIR}/HMI"

inherit pkgconfig qt6-cmake

EXTRA_OECMAKE = " --debug-find-pkg=Qt6Quick "

# wpa_client.so installed manually abd Yocto is confused
INSANE_SKIP:${PN} += "file-rdeps"
