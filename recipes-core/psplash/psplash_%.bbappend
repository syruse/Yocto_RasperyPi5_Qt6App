# Before looking for files in the standard recipe folders, take a look in my files folder first
# my files will be first
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# to generate image header file you need to call
# ~/yocto/build_pi/tmp/work/cortexa76-poky-linux/psplash/0.1+git/git/make-image-header.sh logo.png POKY
SPLASH_IMAGES:rpi = "file://logo-img.h;outsuffix=raspberrypi"

inherit systemd
SYSTEMD_PACKAGES = "${PN}-raspberrypi"
SYSTEMD_SERVICE:${PN}-raspberrypi = "psplash-start.service"

do_install:append:rpi() {
    if [ "${@bb.utils.filter('DISTRO_FEATURES', 'systemd', d)}" ]; then
        install -d ${D}${systemd_system_unitdir}
        # We erase everything that was there and write our own working config
        cat <<EOF > ${D}${systemd_system_unitdir}/psplash-start.service
[Unit]
Description=Start Psplash Boot Screen
DefaultDependencies=no
Requires=dev-fb0.device
After=dev-fb0.device systemd-journald.socket

[Service]
Type=simple
ExecStartPre=/bin/sleep 0.5
ExecStart=/usr/bin/psplash -n

[Install]
WantedBy=sysinit.target
EOF

       # removing the redundant file which was created by psplash bb
       rm -f ${D}${systemd_system_unitdir}/psplash-systemd.service
    fi
}

# for service auto-enabling
SYSTEMD_SERVICE:${PN}-raspberrypi = "psplash-start.service"
# let's keep this service disabled since it uses fb0 while our app uses drm, it may causes blinking
# SYSTEMD_AUTO_ENABLE:${PN}-raspberrypi = "disable"
