#include <jni.h>

#include "classloader.h"

#include "elf_parser.hpp"
#include "maps_scan.hpp"
#include "logging.h"

#include "art.hpp"
#include "reflection.hpp"

#include <memory>
#include <string>
#include <array>

static bool initArt(JNIEnv *env) {
    elf_parser::Elf art{};
    uintptr_t art_base;
    std::string art_path;
    maps_scan::MapInfo::ForEach([&](auto &map) -> bool {
        if (map.path.ends_with("/libart.so") && map.offset == 0) {
            art_base = map.start;
            art_path = map.path;
            return false;
        }
        return true;
    });
    if (!art.InitFromFile(art_path, art_base, true)) {
        LOGE("init art");
        return false;
    }
    bool success = true;
    if (!art::Init(env, art)) {
        LOGE("ArtDebugging init failed");
        success = false;
    }
    success &= InitClassLoaders(art);
    return success;
}

extern "C"
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv *env;
    vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_4);
    bool success = true;
    success &= Reflection::Init(env);
    success &= initArt(env);
    return success ? JNI_VERSION_1_4 : JNI_ERR;
}

struct OatFile {
    virtual ~OatFile() = default;
    const std::string location_;
};

extern "C"
JNIEXPORT jstring JNICALL
Java_io_github_a13e300_tools_NativeUtils_nativeReadOatPath(JNIEnv *env, jclass , jlong addr) {
    return env->NewStringUTF(reinterpret_cast<OatFile*>(addr)->location_.c_str());
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_io_github_a13e300_tools_NativeUtils_nativeJitCompile(JNIEnv *env, jclass clazz,
                                                          jlong art_method, jint compile_kind,
                                                          jboolean prejit) {
    auto jit = art::Runtime::Current()->GetJit();
    LOGD("got jit: %p", jit);
    LOGD("jit first addr: %p", *(void**)jit);
    if (!jit) return JNI_FALSE;
    return jit->CompileMethod(
            reinterpret_cast<art::ArtMethod*>(art_method),
            art::Thread::Current(),
            static_cast<art::CompilationKind>(compile_kind),
            prejit == JNI_TRUE
    ) ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_io_github_a13e300_tools_NativeUtils_nativeGetMethodEntry(JNIEnv *env, jclass clazz,
                                                              jlong art_method) {
    return reinterpret_cast<jlong>(reinterpret_cast<art::ArtMethod*>(art_method)->GetEntryPoint());
}

extern "C"
JNIEXPORT jlong JNICALL
Java_io_github_a13e300_tools_NativeUtils_nativeGetMethodData(JNIEnv *env, jclass clazz,
                                                             jlong art_method) {
    return reinterpret_cast<jlong>(reinterpret_cast<art::ArtMethod*>(art_method)->GetData());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_io_github_a13e300_tools_NativeUtils_nativeGetAddrInfo(JNIEnv *env, jclass clazz, jlong jaddr) {
    char buf[100];
    std::string desc;
    auto addr = static_cast<uintptr_t>(jaddr);
    snprintf(buf, sizeof(buf), "%p", addr);
#if defined(__aarch64__)
    addr &= (1ull << 56) - 1;
#endif
    desc += buf;
    dev_t head_dev = 0;
    ino_t head_ino = 0;
    uintptr_t head_start = 0;
    maps_scan::MapInfo::ForEach([&](const maps_scan::MapInfo& map) {
        if (map.offset == 0 && map.is_private && map.path.starts_with('/')) {
            head_dev = map.dev;
            head_ino = map.inode;
            head_start = map.start;
        }
        if (map.start <= addr && addr < map.end) {
            desc += " in ";
            snprintf(buf, sizeof(buf), "[%p-%p]", map.start, map.end);
            desc += buf;
            snprintf(buf, sizeof(buf), "+0x%lx ", addr - map.start);
            desc += buf;
            desc += map.path;
            if (map.path.ends_with(".so") && map.dev == head_dev && map.inode == head_ino) {
                LOGD("parsing %s %p start %p", map.path.c_str(), map.start, head_start);
                elf_parser::Elf elf{};
                if (elf.InitFromFile(map.path, head_start, true)) {
                    elf.forEachSymbols([&](const char* name, const elf_parser::SymbolInfo sym) -> bool {
                        auto sym_start = sym.value;
                        auto sym_end = sym_start + sym.size;
                        if (sym_start <= addr && addr < sym_end) {
                            desc += ":";
                            desc += sym.name;
                            snprintf(buf, sizeof(buf), "+0x%lx/0x%lx", addr - sym_start, sym.size);
                            desc += buf;
                            return false;
                        }
                        return true;
                    });
                } else {
                    LOGE("init failed");
                }
            }
            return false;
        }
        return true;
    });

    return env->NewStringUTF(desc.c_str());
}


extern "C"
JNIEXPORT void JNICALL
Java_io_github_a13e300_tools_HiddenApiActivity_testHiddenApi(JNIEnv *env, jclass clazz) {
    auto clz = env->FindClass("java/lang/Class");
    auto fld = env->GetFieldID(clz, "status", "I");
    LOGD("got field without SHAA: %p", fld);
    env->ExceptionClear();

    {
        art::ScopedHiddenApiAccess shaa{env};
        auto fld2 = env->GetFieldID(clz, "status", "I");
        LOGD("got field with SHAA: %p", fld2);
        env->ExceptionClear();
    }
}
