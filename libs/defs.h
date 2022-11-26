#ifndef __LIBS_DEFS_H__
#define __LIBS_DEFS_H__

// 定义NULL
#ifndef NULL
#define NULL ((void *)0)
#endif

// 定义bool类型
#define bool _Bool
#define true 1
#define false 0

// 定义定长整型
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

// 能容纳指针长度整型
typedef int32_t intptr_t;
typedef uint32_t uintptr_t;
typedef uintptr_t size_t;

#endif /* !__LIBS_DEFS_H__ */
