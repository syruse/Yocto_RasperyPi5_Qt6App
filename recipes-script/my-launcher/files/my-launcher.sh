#!/bin/sh
#echo "Running my custom script..."

systemctl stop getty@tty1
echo 0 | tee /sys/class/graphics/fbcon/cursor_blink
cat /dev/zero > /dev/fb0

# this is wayland compositor
#export XDG_RUNTIME_DIR=/tmp
#wl-compositor

# run welcome qt app over drm\kms

# this is eglfs config for QT (QT_QPA_EGLFS_KMS_CONFIG)
QT_EGLFS_CONFIG=/tmp/eglfs.json
if [ -f "${QT_EGLFS_CONFIG}" ]; then
   rm "${QT_EGLFS_CONFIG}"
fi
echo '{ "device": "/dev/dri/card0" }' >> ${QT_EGLFS_CONFIG}

export QT_QPA_PLATFORM=eglfs
export QT_QPA_EGLFS_KMS_CONFIG=${QT_EGLFS_CONFIG}
export QT_QPA_EGLFS_HIDECURSOR=1
appHMI

exit 0