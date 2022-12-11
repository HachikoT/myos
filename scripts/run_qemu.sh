CURRENT_DIR=$(cd $(dirname $0); pwd)
#qemu-system-i386 -monitor  stdio -hda ${CURRENT_DIR}/../build/bin/myos.img -drive file=${CURRENT_DIR}/../build/bin/swap.img,media=disk,cache=writeback -drive file=${CURRENT_DIR}/../build/bin/sfs.img,media=disk,cache=writeback -serial null
qemu-system-i386 -S -s -parallel  stdio -hda ${CURRENT_DIR}/../build/bin/myos.img -drive file=${CURRENT_DIR}/../build/bin/swap.img,media=disk,cache=writeback -drive file=${CURRENT_DIR}/../build/bin/sfs.img,media=disk,cache=writeback -serial null
