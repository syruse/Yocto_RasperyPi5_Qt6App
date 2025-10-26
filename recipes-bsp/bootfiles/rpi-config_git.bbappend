#inherit deploy nopackages

#do_deploy:append() {
#    CONFIG=${DEPLOYDIR}/${BOOTFILES_DIR_NAME}/config.txt
#
#    echo "# Enable PCI v3 for nvme" >>$CONFIG
#    echo "dtparam=pciex1=on" >>$CONFIG
#    echo "dtparam=pciex1_gen=3" >>$CONFIG
#}