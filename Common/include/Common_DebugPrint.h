#ifndef COMMON_DEBUGPRINT_H
#define COMMON_DEBUGPRINT_H
#include <stdio.h>

#define _DEBUG_PRINT_INTERNAL(level, format, ...) \
    printf("[%s] File <%s>, at line %d, in <%s>: " format "\n", level,__FILE__,__LINE__,__func__, ##__VA_ARGS__)

// #define ENABLE_PRINT_ERROR
// #define ENABLE_PRINT_WARNING
// #define ENABLE_PRINT_INFO
// #define ENABLE_PRINT_DEBUG

#ifdef ENABLE_PRINT_DEBUG
#define PRINT_DEBUG(format, ...) \
    _DEBUG_PRINT_INTERNAL("DEBUG", format, ##__VA_ARGS__)
#else
#define PRINT_DEBUG(...)
#endif

#ifdef ENABLE_PRINT_INFO
#define PRINT_INFO(format, ...) \   
    _DEBUG_PRINT_INTERNAL("INFO", format, ##__VA_ARGS__)
#else
#define PRINT_INFO(...)
#endif

#ifdef ENABLE_PRINT_WARNING
#define PRINT_WARNING(format, ...) \
    _DEBUG_PRINT_INTERNAL("WARNING", format, ##__VA_ARGS__)
#else
#define PRINT_WARNING(...)
#endif

#ifdef ENABLE_PRINT_ERROR
#define PRINT_ERROR(format, ...) \
    _DEBUG_PRINT_INTERNAL("ERROR", format, ##__VA_ARGS__)
#else
#define PRINT_ERROR(...)
#endif


#endif /* COMMON_DEBUGPRINT_H */