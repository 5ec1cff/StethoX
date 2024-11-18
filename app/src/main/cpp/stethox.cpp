#include <jni.h>

#include "classloader.h"
#include "art.h"

#include "elf_parser.hpp"
#include "maps_scan.hpp"
#include "logging.h"

#include <memory>
#include <string>
#include <memory>
#include <array>

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_io_github_a13e300_tools_NativeUtils_getClassLoaders(JNIEnv *env, jclass clazz) {
    return visitClassLoaders(env);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_io_github_a13e300_tools_NativeUtils_initNative(JNIEnv *env, jclass clazz) {
    return art::Runtime::Init(env);
}
extern "C"
JNIEXPORT jobjectArray JNICALL
Java_io_github_a13e300_tools_NativeUtils_getClassLoaders2(JNIEnv *env, jclass clazz) {
    return visitClassLoadersByRootVisitor(env);
}

static elf_parser::Elf* getArt() {
    static std::unique_ptr<elf_parser::Elf> elf;
    if (!elf) elf = std::make_unique<elf_parser::Elf>();
    if (!elf->IsValid()) {
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
        if (!elf->InitFromFile(art_path, art_base, true)) {
            LOGE("init art");
        }
    }
    if (!elf->IsValid()) {
        return nullptr;
    }
    return elf.get();
}

extern "C"
JNIEXPORT void JNICALL
Java_io_github_a13e300_tools_NativeUtils_nativeAllowDebugging(JNIEnv *env, jclass clazz) {
    auto &elf = *getArt();
    auto addr = elf.getSymbAddress("_ZN3art3Dbg14SetJdwpAllowedEb");
    if (addr == nullptr) {
        LOGE("get _ZN3art3Dbg14SetJdwpAllowedEb");
        return;
    }
    reinterpret_cast<void(*)(bool)>(addr)(true);
    LOGD("allow debug");
}

extern "C"
JNIEXPORT void JNICALL
Java_io_github_a13e300_tools_NativeUtils_nativeSetJavaDebug(JNIEnv *, jclass,
                                                            jboolean allow) {
    // https://github.com/LSPosed/LSPlant/blob/08b65e436f766066f7aff1e35309aee0c656e3ba/lsplant/src/main/jni/art/runtime/runtime.cxx#L65

    // prevent from deopt
    // https://cs.android.com/android/platform/superproject/main/+/main:art/openjdkjvmti/deopt_manager.cc;l=145;drc=6f07c0cc2811994b8b9c350a46534138be423479
    // https://cs.android.com/android/platform/superproject/+/android-13.0.0_r3:art/openjdkjvmti/deopt_manager.cc;l=152;drc=c618f7c01e83409e57f3e74bdc1fd4bdc6a828b5
    enum class RuntimeDebugState {
        // This doesn't support any debug features / method tracing. This is the expected state
        // usually.
        kNonJavaDebuggable,
        // This supports method tracing and a restricted set of debug features (for ex: redefinition
        // isn't supported). We transition to this state when method tracing has started or when the
        // debugger was attached and transition back to NonDebuggable once the tracing has stopped /
        // the debugger agent has detached..
        kJavaDebuggable,
        // The runtime was started as a debuggable runtime. This allows us to support the extended
        // set
        // of debug features (for ex: redefinition). We never transition out of this state.
        kJavaDebuggableAtInit
    };
    struct Runtime;
    static Runtime* runtime = nullptr;
    static size_t java_debuggable_offset = 0;
    static bool orig_javaDebuggable = false;
    static size_t debug_state_offset = 0;
    static RuntimeDebugState orig_debugState = RuntimeDebugState::kNonJavaDebuggable;
    auto &art = *getArt();

    if (runtime == nullptr) {
        runtime = *reinterpret_cast<Runtime**>(art.getSymbAddress("_ZN3art7Runtime9instance_E"));
    }

    if (debug_state_offset == 0) {
        auto fn = art.getSymbAddress("_ZN3art7Runtime20SetRuntimeDebugStateENS0_17RuntimeDebugStateE");
        if (fn && runtime) {
            static constexpr size_t kLargeEnoughSizeForRuntime = 4096;
            std::array<uint8_t, kLargeEnoughSizeForRuntime> code;
            static_assert(static_cast<int>(RuntimeDebugState::kJavaDebuggable) != 0);
            static_assert(static_cast<int>(RuntimeDebugState::kJavaDebuggableAtInit) != 0);
            code.fill(uint8_t{0});
            auto *const fake_runtime = reinterpret_cast<Runtime *>(code.data());
            reinterpret_cast<void(*)(Runtime*, RuntimeDebugState)>(fn)(fake_runtime, RuntimeDebugState::kJavaDebuggable);
            for (size_t i = 0; i < kLargeEnoughSizeForRuntime; ++i) {
                if (*reinterpret_cast<RuntimeDebugState *>(
                        reinterpret_cast<uintptr_t>(fake_runtime) + i) ==
                    RuntimeDebugState::kJavaDebuggable) {
                    LOGD("found debug_state at offset %zu", i);
                    debug_state_offset = i;
                    break;
                }
            }
            if (debug_state_offset == 0) {
                LOGE("failed to find debug_state");
                debug_state_offset = -1;
            }
        }
        else debug_state_offset = -1;
    }

    if (java_debuggable_offset == 0) {
        auto fn = art.getSymbAddress("_ZN3art7Runtime17SetJavaDebuggableEb");
        if (fn && runtime) {
            static constexpr size_t kLargeEnoughSizeForRuntime = 4096;
            std::array<uint8_t, kLargeEnoughSizeForRuntime> code;
            static_assert(static_cast<int>(RuntimeDebugState::kJavaDebuggable) != 0);
            static_assert(static_cast<int>(RuntimeDebugState::kJavaDebuggableAtInit) != 0);
            code.fill(uint8_t{0});
            auto *const fake_runtime = reinterpret_cast<Runtime *>(code.data());
            reinterpret_cast<void(*)(Runtime*, bool)>(fn)(fake_runtime, true);
            for (size_t i = 0; i < kLargeEnoughSizeForRuntime; ++i) {
                if (*reinterpret_cast<bool*>(
                        reinterpret_cast<uintptr_t>(fake_runtime) + i)) {
                    LOGD("found java_debuggable at offset %zu", i);
                    java_debuggable_offset = i;
                    break;
                }
            }
            if (java_debuggable_offset == 0) {
                LOGE("failed to find debug_state");
                java_debuggable_offset = -1;
            }
        } else java_debuggable_offset = -1;
    }

    if (!runtime) {
        LOGE("no runtime");
        return;
    }

    if (debug_state_offset != -1) {
        auto addr = reinterpret_cast<RuntimeDebugState*>(reinterpret_cast<uintptr_t>(runtime) + debug_state_offset);
        if (allow) {
            orig_debugState = *addr;
            LOGD("setRuntimeDebugState allow orig=%d", (int) orig_debugState);
            *addr = RuntimeDebugState::kJavaDebuggable;
        } else {
            LOGD("setRuntimeDebugState restore orig=%d", (int) orig_debugState);
            *addr = orig_debugState;
        }
        return;
    }

    if (java_debuggable_offset != -1) {
        auto addr = reinterpret_cast<bool*>(reinterpret_cast<uintptr_t>(runtime) + java_debuggable_offset);
        if (allow) {
            orig_javaDebuggable = *addr;
            LOGD("setJavaDebuggable allow orig=%d", orig_javaDebuggable);
            *addr = true;
        } else {
            LOGD("setJavaDebuggable restore orig=%d", orig_javaDebuggable);
            *addr = orig_javaDebuggable;
        }
        return;
    }

    LOGE("no method to set runtime debug");
}
