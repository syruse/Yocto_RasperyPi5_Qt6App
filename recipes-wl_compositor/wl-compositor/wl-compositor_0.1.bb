SUMMARY = "Wayland compositor"
SECTION = "compositors"
LICENSE = "CLOSED"

DEPENDS += " cmake pkgconfig wlroots "

RDEPENDS${PN} += "wlroots"

SRC_URI = "file://CMakeLists.txt \
           file://wl_compositor.cpp \
           file://wl_protocols/xdg-shell-protocol.h \
           file://wl_protocols/xdg-shell-protocol.c \
           "

S = "${UNPACKDIR}"

#EXAMPLE HOW TO PASS OPTIONS INTO CMAKE
#EXTRA_OECMAKE = "-DENABLE_FEATURE=ON"

CFLAGS += " -I${STAGING_INCDIR}/wlroots-0.19 "
LDFLAGS += " -L${STAGING_LIBDIR} "

inherit pkgconfig cmake

do_install() {
        install -d ${D}${bindir}
        install -m 0755 wl-compositor ${D}${bindir}/wl-compositor
}

FILES:${PN} += "${bindir}/wl-compositor"
