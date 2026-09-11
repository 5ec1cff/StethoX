#include <jni.h>
#include <map>
#include <string>
#include <shared_mutex>
#include <atomic>
#include <memory>
#include <vector>
#include "art.hpp"
#include "logging.h"

static thread_local bool in_string_constructor{};

static jclass reportClass;
static jmethodID reportMethodID;

static constexpr const int STRING_TRACE_FLAG_EQUALS = 1;
static constexpr const int STRING_TRACE_FLAG_STARTSWITH = 1 << 1;
static constexpr const int STRING_TRACE_FLAG_ENDSWITH = 1 << 2;
static constexpr const int STRING_TRACE_FLAG_CONTAINS = 1 << 3;
static constexpr const int STRING_TRACE_FLAG_REGEX = 1 << 4;
static constexpr const int STRING_TRACE_FLAG_IGNORECASE = 1 << 5;
static constexpr const int STRING_TRACE_FLAGS_ALL = STRING_TRACE_FLAG_EQUALS | STRING_TRACE_FLAG_STARTSWITH | STRING_TRACE_FLAG_ENDSWITH | STRING_TRACE_FLAG_CONTAINS | STRING_TRACE_FLAG_REGEX | STRING_TRACE_FLAG_IGNORECASE;

struct StringTrace {
    std::string keyword;
    int id;
    int flag;
    int report_from_count;
    int total_count;
    std::atomic<int> already_hit_count{};
};

static int trace_mutex_id;
static std::shared_mutex trace_mutex{};
static std::map<int, std::unique_ptr<StringTrace>> traces{};

static void handleStringTrace(JNIEnv *env, jstring str);

#define DEF_HOOK_FN(rettype, name, ...) \
    static rettype (*orig_##name)(__VA_ARGS__) = nullptr; \
    static rettype hook_##name(__VA_ARGS__)

DEF_HOOK_FN(jstring, newStringFromBytes, JNIEnv* env, jclass clz, jbyteArray java_data, jint high, jint offset, jint byte_count) {
    auto s = orig_newStringFromBytes(env, clz, java_data, high, offset, byte_count);
    if (s) {
        handleStringTrace(env, s);
    }
    return s;
}

DEF_HOOK_FN(jstring, newStringFromUtf16Bytes, JNIEnv* env, jclass clz, jbyteArray java_data, jint offset, jint char_count) {
    auto s = orig_newStringFromUtf16Bytes(env, clz, java_data, offset, char_count);
    if (s) {
        handleStringTrace(env, s);
    }
    return s;
}

DEF_HOOK_FN(jstring, newStringFromChars, JNIEnv* env, jclass clz, jint offset, jint char_count, jcharArray java_data) {
    auto s = orig_newStringFromChars(env, clz, offset, char_count, java_data);
    if (s) {
        handleStringTrace(env, s);
    }
    return s;
}

DEF_HOOK_FN(jstring, newStringFromString, JNIEnv* env, jclass clz, jstring to_copy) {
    auto s = orig_newStringFromString(env, clz, to_copy);
    if (s) {
        handleStringTrace(env, s);
    }
    return s;
}

DEF_HOOK_FN(jstring, newStringFromUtf8Bytes, JNIEnv* env, jclass clz, jbyteArray java_data, jint offset, jint byte_count) {
    auto s = orig_newStringFromUtf8Bytes(env, clz, java_data, offset, byte_count);
    if (s) {
        handleStringTrace(env, s);
    }
    return s;
}

struct NativeMethod {
    const char *name;
    const char *signature;
    void *hook;
    void **backup;
    art::ArtMethod *method;
};

// TODO: fastSubstring, concat, doReplace, doRepeat in String
#define DEF_NATIVE_METHOD(n, sig) \
    { .name = #n, .signature = sig, .hook = (void *) hook_##n, .backup = (void **) &orig_##n }
static NativeMethod methods_to_hook[] = {
    DEF_NATIVE_METHOD(newStringFromBytes, "([BIII)Ljava/lang/String;"),
    DEF_NATIVE_METHOD(newStringFromChars, "(II[C)Ljava/lang/String;"),
    DEF_NATIVE_METHOD(newStringFromString, "(Ljava/lang/String;)Ljava/lang/String;"),
    DEF_NATIVE_METHOD(newStringFromUtf8Bytes, "([BII)Ljava/lang/String;"),
    DEF_NATIVE_METHOD(newStringFromUtf16Bytes, "([BII)Ljava/lang/String;"),
};

static bool hookStringFactory(JNIEnv *env) {
    LOGD("start hook StringFactory");
    for (auto &m: methods_to_hook) {
        auto &nativeEntry = m.method->NativeEntry();
        *m.backup = nativeEntry;
        nativeEntry = m.hook;
    }
    return true;
}

static void unhookStringFactory(JNIEnv *env) {
    LOGD("unhook StringFactory");
    for (auto &m: methods_to_hook) {
        auto &nativeEntry = m.method->NativeEntry();
        nativeEntry = *m.backup;
    }
}

static void handleStringTrace(JNIEnv *env, jstring str) {
    if (in_string_constructor || env->ExceptionCheck()) return;

    in_string_constructor = true;

    auto cstr = env->GetStringUTFChars(str, nullptr);
    //LOGV("new str: %s", cstr);
    std::vector<int> report_ids_and_hits{}, remove_ids{};

    {
        std::shared_lock lk{trace_mutex};
        for (auto &[id, trace]: traces) {
            auto flag = trace->flag;
            bool match = false;
            if (flag & STRING_TRACE_FLAG_EQUALS) {
                if ((flag & STRING_TRACE_FLAG_IGNORECASE) != 0) {
                    match = strcasecmp(cstr, trace->keyword.c_str()) == 0;
                } else {
                    match = strcmp(cstr, trace->keyword.c_str()) == 0;
                }
            } else if (flag & STRING_TRACE_FLAG_CONTAINS) {
                if ((flag & STRING_TRACE_FLAG_IGNORECASE) != 0) {
                    match = strcasestr(cstr, trace->keyword.c_str()) != nullptr;
                } else {
                    match = strstr(cstr, trace->keyword.c_str()) != nullptr;
                }
            } else if (flag & STRING_TRACE_FLAG_STARTSWITH) {
                size_t len = strlen(cstr);
                auto kwlen = trace->keyword.size();
                if (len < kwlen) match = false;
                else {
                    if ((flag & STRING_TRACE_FLAG_IGNORECASE) != 0) {
                        match = strncasecmp(cstr, trace->keyword.c_str(), kwlen) == 0;
                    } else {
                        match = strncmp(cstr, trace->keyword.c_str(), kwlen) == 0;
                    }
                }
            } else if (flag & STRING_TRACE_FLAG_ENDSWITH) {
                size_t len = strlen(cstr);
                auto kwlen = trace->keyword.size();
                if (len < kwlen) match = false;
                else {
                    auto end = cstr + len - kwlen;
                    if ((flag & STRING_TRACE_FLAG_IGNORECASE) != 0) {
                        match = strncasecmp(end, trace->keyword.c_str(), kwlen);
                    } else {
                        match = strncmp(end, trace->keyword.c_str(), kwlen);
                    }
                }
            } else {
                // TODO: regex
            }

            if (match) {
                auto hit = trace->already_hit_count.fetch_add(1);
                if (hit >= trace->total_count - 1) {
                    remove_ids.push_back(id);
                }
                if (hit >= trace->report_from_count && hit < trace->total_count) {
                    report_ids_and_hits.push_back(id);
                    report_ids_and_hits.push_back(hit);
                }
            }
        }
    }

    env->ReleaseStringUTFChars(str, cstr);

    if (!report_ids_and_hits.empty()) {
        auto arr = env->NewIntArray(report_ids_and_hits.size());
        auto buf = env->GetIntArrayElements(arr, nullptr);
        memcpy(buf, report_ids_and_hits.data(), sizeof(int) * report_ids_and_hits.size());
        env->ReleaseIntArrayElements(arr, buf, 0);

        env->CallStaticVoidMethod(reportClass, reportMethodID, str, arr);
        if (env->ExceptionCheck()) {
            LOGW("exception occurred in StringTrace!");
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
    }

    if (!remove_ids.empty()) {
        std::unique_lock lk2{trace_mutex};
        for (auto id: remove_ids) {
            traces.erase(id);
        }
        if (traces.empty()) {
            unhookStringFactory(env);
        }
    }

    in_string_constructor = false;
}

extern "C"
JNIEXPORT jint JNICALL
Java_io_github_a13e300_tools_StringTrace_registerStringTraceNative(JNIEnv *env, jclass clazz,
                                                             jstring keyword, jint flag,
                                                             jint report_from_count,
                                                             jint total_count) {
    if ((flag & (~STRING_TRACE_FLAGS_ALL)) != 0) {
        return -1;
    }
    if (keyword == nullptr) {
        return -1;
    }
    if (report_from_count < 0 || report_from_count >= total_count || total_count <= 0) {
        return -1;
    }
    auto str = env->GetStringUTFChars(keyword, nullptr);
    std::string kw{str};
    env->ReleaseStringUTFChars(keyword, str);
    std::unique_lock lk{trace_mutex};
    if (traces.empty()) {
        if (!hookStringFactory(env)) {
            return -1;
        }
    }
    auto id = ++trace_mutex_id;
    traces.emplace(id, std::make_unique<StringTrace>(kw, id, flag, report_from_count, total_count));
    return id;
}

extern "C"
JNIEXPORT void JNICALL
Java_io_github_a13e300_tools_StringTrace_unregisterStringTraceNative(JNIEnv *env, jclass clazz, jint id) {
    std::unique_lock lk{trace_mutex};
    auto origEmpty = traces.empty();
    if (auto it = traces.find(id); it != traces.end()) {
        traces.erase(it);
    }
    if (!origEmpty && traces.empty()) {
        unhookStringFactory(env);
    }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_io_github_a13e300_tools_StringTrace_setupStringTraceNative(JNIEnv *env, jclass clazz) {
    if (!art::ArtMethod::Init(env)) {
        LOGE("init ArtMethod failed");
        return JNI_FALSE;
    }
    auto sf = env->FindClass("java/lang/StringFactory");
    for (auto &m: methods_to_hook) {
        auto id = env->GetStaticMethodID(sf, m.name, m.signature);
        if (!id) {
            LOGE("GetMethodID StringFactory %s failed", m.name);
            return JNI_FALSE;
        }
        auto artMethod = art::ArtMethod::FromJMethod(env, sf, id, true);
        m.method = artMethod;
    }
    auto clz = env->FindClass("io/github/a13e300/tools/StringTrace");
    if (clz == nullptr) {
        return JNI_FALSE;
    }
    reportMethodID = env->GetStaticMethodID(clz, "reportStringTrace", "(Ljava/lang/String;[I)V");
    if (!reportMethodID) {
        return JNI_FALSE;
    }
    reportClass = (jclass) env->NewGlobalRef(clz);
    if (!reportClass) {
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

extern "C"
JNIEXPORT void JNICALL
Java_io_github_a13e300_tools_StringTrace_markStringTraceIgnore(JNIEnv *env, jclass clazz,
                                                               jboolean ignore) {
    in_string_constructor = ignore == JNI_TRUE;
}