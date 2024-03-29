# 目录
TOP_DIR := $(patsubst %/,%, $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
BUILD_DIR := $(TOP_DIR)/build
BIN_DIR := $(TOP_DIR)/build/bin
export TOP_DIR BUILD_DIR BIN_DIR

# 编译器
CC := gcc
CFLAGS := -Wall -g -O2 -m32 -std=gnu99 -fno-builtin -nostdinc -fno-stack-protector -I $(TOP_DIR)
export CC CFLAGS

# 链接
LD := ld
LDFLAGS	:= -m elf_i386 -nostdlib
export LD LDFLAGS

# 打包
AR := ar
ARFLAGS := -rc
export AR ARFLAGS

# myos
MYOS_TARGET := $(BIN_DIR)/myos.img
MYOS_MODULES := boot kernel swap sfs

.PHONY:myos
myos:$(MYOS_TARGET)

$(MYOS_TARGET):$(MYOS_MODULES) | $(BIN_DIR)
	@echo -e "\e[32m""Generating image $@""\e[0m"
	@dd if=/dev/zero of=/home/rc/work/myos/build/bin/myos.img count=10000
	@dd if=$(BIN_DIR)/boot of=$@ conv=notrunc
	@dd if=$(BIN_DIR)/kernel of=$@ seek=1 conv=notrunc

# boot
BOOT_TARGET := $(BIN_DIR)/boot
BOOT_MODULES := sign boot
BOOT_LIBS := $(foreach n, $(BOOT_MODULES), $(BUILD_DIR)/$(n)/lib/$(n))
BOOT_MODULE = $(basename $(notdir $@))

.PHONY:boot
boot:$(BOOT_TARGET)

$(BOOT_TARGET):$(BOOT_LIBS) | $(BIN_DIR)
	@echo -e "\e[32m""Signing executable $@""\e[0m"
	@$(BUILD_DIR)/sign/lib/sign $(BUILD_DIR)/boot/lib/boot $@

$(BOOT_LIBS)::
	@make -s -f $(TOP_DIR)/boot/$(BOOT_MODULE)/makefile MODULE=$(BOOT_MODULE)

# kernel
KERNEL_TARGET := $(BIN_DIR)/kernel
KERNEL_MODULES := init syscall trap schedule process fs mm sync debug driver libs
KERNEL_LIBS := $(foreach n, $(KERNEL_MODULES), $(BUILD_DIR)/$(n)/lib/lib$(n).a)
KERNEL_MODULE = $(patsubst lib%.a,%, $(notdir $@))

.PHONY:kernel
kernel:$(KERNEL_TARGET)

$(KERNEL_TARGET):$(KERNEL_LIBS) | $(BIN_DIR)
	@echo -e "\e[32m""Linking executable $@""\e[0m"
	@$(LD) $(LDFLAGS) -T $(TOP_DIR)/scripts/kernel.ld -o $@ --start-group $(KERNEL_LIBS) --end-group

$(KERNEL_LIBS)::
	@make -s -f $(TOP_DIR)/$(if $(filter-out libs,$(KERNEL_MODULE)),kern/)$(KERNEL_MODULE)/makefile MODULE=$(KERNEL_MODULE)

# swap
SWAP_TARGET := $(BIN_DIR)/swap.img

.PHONY:swap
swap:${SWAP_TARGET}

${SWAP_TARGET}:
	@dd if=/dev/zero of=$@ bs=1M count=128

# sfs
SFS_TARGET := $(BIN_DIR)/sfs.img
SFS_ROOT := $(BIN_DIR)/sfs_root

.PHONY:sfs
sfs:${SFS_TARGET}

${SFS_TARGET}:user
	@mkdir -p ${SFS_ROOT}
	@make -s -f $(TOP_DIR)/kern/mksfs/makefile MODULE=mksfs
	@cp $(BIN_DIR)/user ${SFS_ROOT}/user
	@dd if=/dev/zero of=$@ bs=1M count=128
	@${BUILD_DIR}/mksfs/lib/mksfs $@ ${SFS_ROOT}

# user
USER_TARGET := $(BIN_DIR)/user

.PHONY:user
user:${USER_TARGET}

${USER_TARGET}:
	@make -s -f $(TOP_DIR)/user/libs/makefile MODULE=user
	@cp ${BUILD_DIR}/user/lib/user $@

# 自动生成bin目录
$(BIN_DIR):
	@mkdir -p $@

# 清除命令，直接删掉build文件夹
.PHONY:clean
clean:
	@echo -e "\e[31m""Remove build files""\e[0m"
	@rm -rf $(BUILD_DIR)