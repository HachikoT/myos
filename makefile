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

# bootloader
BOOT_MODULES := sign boot

.PHONY:boot
boot:FORCE | $(BIN_DIR)
	@for n in $(BOOT_MODULES); do make -s -f $(TOP_DIR)/boot/$$n/makefile MODULE=$$n || exit "$$?"; done
	@echo -e "\e[32m""Signing executable $(BIN_DIR)/boot""\e[0m"
	@$(BUILD_DIR)/sign/lib/sign $(BUILD_DIR)/boot/lib/boot $(BIN_DIR)/boot

# kernel
KERNEL_MODULES := init trap mm fs debug driver
TOOL_LIB := $(BUILD_DIR)/libs/lib/liblibs.a
KERNEL_LIBS := $(foreach n, $(KERNEL_MODULES), $(BUILD_DIR)/$(n)/lib/lib$(n).a)

.PHONY:kernel
kernel:FORCE | $(BIN_DIR)
	@make -s -f $(TOP_DIR)/libs/makefile MODULE=libs
	@for n in $(KERNEL_MODULES); do make -s -f $(TOP_DIR)/kern/$$n/makefile MODULE=$$n || exit "$$?"; done
	@echo -e "\e[32m""Linking executable $(BIN_DIR)/kernel""\e[0m"
	@$(LD) $(LDFLAGS) -T $(TOP_DIR)/scripts/kernel.ld -o $(BIN_DIR)/kernel $(KERNEL_LIBS) $(TOOL_LIB)

# myos
.PHONY:myos
myos:boot kernel | $(BIN_DIR)
	@echo -e "\e[32m""Generating image $(BIN_DIR)/myos.img""\e[0m"
	@dd if=/dev/zero of=/home/rc/work/myos/build/bin/myos.img count=10000
	@dd if=$(BIN_DIR)/boot of=$(BIN_DIR)/myos.img conv=notrunc
	@dd if=$(BIN_DIR)/kernel of=$(BIN_DIR)/myos.img seek=1 conv=notrunc

# 始终执行
.PHONY:FORCE
FORCE:

# 自动生成bin目录
$(BIN_DIR):
	@mkdir -p $@

# 清除命令，直接删掉build文件夹
.PHONY:clean
clean:
	@echo -e "\e[31m""Remove build files""\e[0m"
	@rm -rf $(BUILD_DIR)