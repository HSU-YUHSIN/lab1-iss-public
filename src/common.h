#ifndef __COMMON_H__
#define __COMMON_H__

#include <assert.h>
#include <stdio.h>

// offsetof() macro
#define offsetof(type, member) __builtin_offsetof(type, member)

/* container_of() - Calculate address of object that contains address ptr
 * @ptr: pointer to member variable
 * @type: type of the structure containing ptr
 * @member: name of the member variable in struct @type
 *
 * Return: @type pointer of object containing ptr
 */
#define container_of(ptr, type, member)                             \
    __extension__({                                                 \
        const __typeof__(((type *)0)->member) *(__pmember) = (ptr); \
        (type *)((char *)__pmember - offsetof(type, member));       \
    })

/* macros for assertion and debugging */
#define Assert(cond, format, ...)                                 \
    do {                                                          \
        if (!(cond)) {                                            \
            fprintf(stderr,                                       \
                    format ", failure at %s:%d/%s()"              \
                           "\n",                                  \
                    ##__VA_ARGS__, __FILE__, __LINE__, __func__); \
            assert(cond);                                         \
        }                                                         \
    } while (0)
#define Perror(cond, format, ...) \
    Assert(cond, format ": %s", ##__VA_ARGS__, strerror(errno))
#define Panic(...) Assert(0, ##__VA_ARGS__)

/* likely() and unlikely() macro */
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

/* LOG() macro */
#define LOG(format, ...) printf("[LOG MESSAGE] " format, ##__VA_ARGS__)

#endif
