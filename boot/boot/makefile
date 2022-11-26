# 目录
SRC_DIR := $(TOP_DIR)/boot/$(MODULE)
DEP_DIR := $(BUILD_DIR)/$(MODULE)/.dep
OBJ_DIR := $(BUILD_DIR)/$(MODULE)/obj
LIB_DIR := $(BUILD_DIR)/$(MODULE)/lib

# 源文件以及中间目标文件和依赖文件
SRCS := $(notdir $(wildcard $(SRC_DIR)/*.S $(SRC_DIR)/*.c))
OBJS := $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(basename $(SRCS))))
DEPS := $(addprefix $(DEP_DIR)/, $(addsuffix .d, $(basename $(SRCS))))

# 自动生成头文件依赖选项
DEPFLAGS=-MT $@ -MMD -MP -MF $(DEP_DIR)/$*.d

# 构建目标
TARGET := $(LIB_DIR)/$(MODULE)
.PHONY:all
all:$(TARGET)

# 生成构建目标
$(TARGET):$(OBJS) | $(LIB_DIR)
	@echo -e "\e[36m""Linking executable $(TARGET)""\e[0m"
	@$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 $^ -o $@
	@objcopy -S -R .eh_frame -R .comment -O binary $@

# 生成中间目标文件
$(OBJ_DIR)/%.o:$(SRC_DIR)/%.S | $(OBJ_DIR) $(DEP_DIR)
	@echo -e "\e[33m""Building object $@""\e[0m"
	@$(CC) -c $(DEPFLAGS) $(CFLAGS) -o $@ $<

$(OBJ_DIR)/%.o:$(SRC_DIR)/%.c | $(OBJ_DIR) $(DEP_DIR)
	@echo -e "\e[33m""Building object $@""\e[0m"
	@$(CC) -c $(DEPFLAGS) $(CFLAGS) -o $@ $<

# 引入中间目标文件头文件依赖关系
include $(wildcard $(DEPS))

# 自动生成lib目录
$(LIB_DIR):
	@mkdir -p $@

# 自动生成obj目录
$(OBJ_DIR):
	@mkdir -p $@

# 自动生成.dep目录
$(DEP_DIR):
	@mkdir -p $@

