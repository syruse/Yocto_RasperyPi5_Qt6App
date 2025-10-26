#ROOTFS_POSTPROCESS_COMMAND += "remove_tty1_service; "
#
#remove_tty1_service () {
#  rm ${D}${sysconfdir}/systemd/system/getty.target.wants/getty@tty1.service
#}
