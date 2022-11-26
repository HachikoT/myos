# 目录
TOP_DIR:=$(patsubst %/, %, $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
BUILD_DIR:=$(TOP_DIR)/build
BIN_DIR:=$(TOP_DIR)/build/bin
export TOP_DIR BUILD_DIR BIN_DIR

# 编译器
CC := gcc
CFLAGS := -Wall -g -O2 -m32 -std=c99 -fno-builtin -nostdinc -fno-stack-protector -I $(TOP_DIR)
export CC CFLAGS

# 链接
LD := ld
LDFLAGS	:= -m elf_i386 -nostdlib
export LD LDFLAGS

# bootloader
BOOT_MODULES := sign boot

.PHONY:boot
boot:FORCE | $(BIN_DIR)
	@for n in $(BOOT_MODULES); do make -s -f $(TOP_DIR)/boot/$$n/makefile MODULE=$$n || exit "$$?"; done
	@echo -e "\e[32m""Signing executable $(BIN_DIR)/boot""\e[0m"
	@$(BUILD_DIR)/sign/lib/sign $(BUILD_DIR)/boot/lib/boot $(BIN_DIR)/boot

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