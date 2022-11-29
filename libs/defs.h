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

// Round down to the nearest multiple of n
#define ROUND_DOWN(a, n) ({       \
    size_t __a = (size_t)(a);     \
    (typeof(a))(__a - __a % (n)); \
})

// Round up to the nearest multiple of n
#define ROUND_UP(a, n) ({                                \
    size_t __n = (size_t)(n);                            \
    (typeof(a))(ROUND_DOWN((size_t)(a) + __n - 1, __n)); \
})

/* Return the offset of 'member' relative to the beginning of a struct type */
#define offsetof(type, member) \
    ((size_t)(&((type *)0)->member))

/* *
 * to_struct - get the struct from a ptr
 * @ptr:    a struct pointer of member
 * @type:   the type of the struct this is embedded in
 * @member: the name of the member within the struct
 * */
#define to_struct(ptr, type, member) \
    ((type *)((char *)(ptr)-offsetof(type, member)))

#endif /* !__LIBS_DEFS_H__ */
