#ifndef VM_LOG_H
#define VM_LOG_H

#include <stdio.h>
#include <stdarg.h>

#if defined(VM_LOG_COLORS_ENABLE)
  #define LOG_COLOR_RESET  "\033[0m"
  #define LOG_COLOR_INFO   "\033[36m" /* Cyan */
  #define LOG_COLOR_WARN   "\033[33m" /* Yellow */
  #define LOG_COLOR_ERROR  "\033[31m" /* Red */
#else
  #define LOG_COLOR_RESET  ""
  #define LOG_COLOR_INFO   ""
  #define LOG_COLOR_WARN   ""
  #define LOG_COLOR_ERROR  ""
#endif

/* Core internal formatted print function */
static inline void vm_log_vfmt(const char* prefix, const char* color, const char* title, const char* fmt, va_list args) {
    #if defined(VM_DEBUG_ENABLE)
    printf("%s[%s] %s: ", color, prefix, title ? title : "VM");
    vprintf(fmt, args);
    printf("%s\n", LOG_COLOR_RESET);
    #else
    (void)prefix; (void)color; (void)title; (void)fmt; (void)args;
    #endif
}

/* 1. Formatted Logging Functions (Supports printf syntax in msg) */

static inline void vm_info(const char* title, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vm_log_vfmt("INFO", LOG_COLOR_INFO, title, fmt, args);
    va_end(args);
}

static inline void vm_warn(const char* title, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vm_log_vfmt("WARN", LOG_COLOR_WARN, title, fmt, args);
    va_end(args);
}

static inline void vm_error(const char* title, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vm_log_vfmt("ERROR", LOG_COLOR_ERROR, title, fmt, args);
    va_end(args);
}

#endif /* VM_LOG_H */
