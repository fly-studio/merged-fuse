#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#define debug_print(fmt, ...) \
        do { fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)

#ifdef WINDOWS
    #include <direct.h>
    #define GetCurrentDir _getcwd
#else
    #include <unistd.h>
    #define GetCurrentDir getcwd
#endif

#define FUSE_USE_VERSION 30
#define _FILE_OFFSET_BITS 64

#endif // COMMON_H_INCLUDED
