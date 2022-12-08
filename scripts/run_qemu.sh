CURRENT_DIR=$(cd $(dirname $0); pwd)
qemu-system-i386 -monitor stdio -hda ${CURRENT_DIR}/../build/bin/myos.img -serial null