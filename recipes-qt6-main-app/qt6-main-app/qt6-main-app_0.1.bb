SUMMARY = "qt6 based main app"
LICENSE = "CLOSED"

DEPENDS += " cmake qtbase qtdeclarative qtdeclarative-native "
RDEPENDS${PN} += " qtbase qtdeclarative "

SRC_URI = "file://HMI "
S = "${UNPACKDIR}/HMI"

inherit pkgconfig qt6-cmake

EXTRA_OECMAKE = " --debug-find-pkg=Qt6Quick "
