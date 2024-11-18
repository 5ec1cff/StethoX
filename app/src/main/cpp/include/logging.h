#pragma once

#include <android/log.h>
#include <cerrno>

#ifdef __cplusplus
extern "C" {
#endif

#define TAG "StethoX"

#define LOGI(...) (__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))
#define LOGW(...) (__android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__))
#define LOGE(...) (__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))
#define PLOGE(fmt, ...) (__android_log_print(ANDROID_LOG_ERROR, TAG, "failed with %d %s: " fmt, errno, strerror(errno) __VA_OPT__(,) __VA_ARGS__))

#ifdef NDEBUG
#define LOGV(...)
#define LOGD(...)
#define debug(...)
#else
#define DEBUG 1
#define LOGV(...) (__android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__))
#define LOGD(...) (__android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__))

#endif

#ifdef __cplusplus
}
#endif
