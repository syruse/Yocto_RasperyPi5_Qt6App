#!/bin/sh
#echo "Running my custom script..."

# Allow the user to see the logo for 3s
# sleep 3

#systemctl stop getty@tty1
#echo 0 | tee /sys/class/graphics/fbcon/cursor_blink

# Optional: we have underlying fb0 layer with splash screen 
# which stays on the bg until shutdown and takes some RAM ~4MB (fb0 for splash and drm for qt)
# psplash-write "QUIT" || killall psplash
# cat /dev/zero > /dev/fb0

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
# for virtual keyboard support
export QT_IM_MODULE=qtvirtualkeyboard
appHMI

exit 0