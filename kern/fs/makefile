# 目录
SRC_DIR := $(TOP_DIR)/kern/$(MODULE)
DEP_DIR := $(BUILD_DIR)/$(MODULE)/.dep
OBJ_DIR := $(BUILD_DIR)/$(MODULE)/obj
LIB_DIR := $(BUILD_DIR)/$(MODULE)/lib

# 源文件以及中间目标文件和依赖文件
SRC_DIRS := $(shell find $(SRC_DIR) -maxdepth 3 -type d)
DEP_DIRS := $(patsubst $(SRC_DIR)%,$(DEP_DIR)%,$(SRC_DIRS))
OBJ_DIRS := $(patsubst $(SRC_DIR)%,$(OBJ_DIR)%,$(SRC_DIRS))
SRCS := $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))
OBJS := $(patsubst $(SRC_DIR)%.c,$(OBJ_DIR)%.o, $(SRCS))
DEPS := $(patsubst $(SRC_DIR)%.c,$(DEP_DIR)%.d, $(SRCS))

# 自动生成头文件依赖选项
DEPFLAGS=-MT $@ -MMD -MP -MF $(DEP_DIR)/$*.d

# 构建目标
TARGET := $(LIB_DIR)/lib$(MODULE).a
.PHONY:all
all:$(TARGET)

# 生成构建目标
$(TARGET):$(OBJS) | $(LIB_DIR)
	@echo -e "\e[36m""Archiving library $(TARGET)""\e[0m"
	@$(AR) $(ARFLAGS) $@ $^

# 生成中间目标文件
$(OBJ_DIR)/%.o:$(SRC_DIR)/%.c | $(OBJ_DIRS) $(DEP_DIRS)
	@echo -e "\e[33m""Building object $@""\e[0m"
	@$(CC) -c $(DEPFLAGS) $(CFLAGS) -o $@ $<

# 引入中间目标文件头文件依赖关系
include $(wildcard $(DEPS))

# 自动生成lib目录
$(LIB_DIR):
	@mkdir -p $@

# 自动生成obj目录
$(OBJ_DIRS):
	@mkdir -p $@

# 自动生成.dep目录
$(DEP_DIRS):
	@mkdir -p $@

