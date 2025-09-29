#include <jni.h>

#include "logging.h"

#include "utils.h"
#include <string>

#include <memory>
#include <string>
#include <memory>
#include <array>
#include "art.hpp"
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace art {
    void (*ClassLinker::visit_class_loader_)(void *, ClassLoaderVisitor *) = nullptr;
    void (*ClassLinker::visit_classes_)(void*, ClassVisitor *) = nullptr;
    RuntimeCallbacks* (*get_runtime_callbacks_)(Runtime*) = nullptr;
    void (*add_class_load_callback_)(RuntimeCallbacks*, ClassLoadCallback*) = nullptr;
    void (*remove_class_load_callback_)(RuntimeCallbacks*, ClassLoadCallback*) = nullptr;
    ReaderWriterMutex** classlinker_class_lock_ptr = nullptr;

    bool ClassLinker::Init(elf_parser::Elf &art) {
        visit_class_loader_ = reinterpret_cast<decltype(visit_class_loader_)>(
                art.getSymbAddress("_ZNK3art11ClassLinker17VisitClassLoadersEPNS_18ClassLoaderVisitorE"));
        LOGD("VisitClassLoaders: %p", visit_class_loader_);
        visit_classes_ = reinterpret_cast<decltype(visit_classes_)>(
                art.getSymbAddress("_ZN3art11ClassLinker12VisitClassesEPNS_12ClassVisitorE"));
        LOGD("VisitClasses: %p", visit_classes_);
        return visit_class_loader_ && visit_classes_;
    }

    void ClassLinker::VisitClassLoaders(ClassLoaderVisitor *clv) {
        if (visit_class_loader_) visit_class_loader_(this, clv);
    }

    void ClassLinker::VisitClasses(art::ClassVisitor *visitor) {
        if (visit_classes_) visit_classes_(this, visitor);
    }

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

    inline static unsigned int class_linker_offset_;
    inline static size_t java_debuggable_offset = -1;
    inline static size_t debug_state_offset = -1;

    Runtime *Runtime::instance_ = nullptr;
    bool Runtime::Init(JNIEnv *env, elf_parser::Elf &art) {
        // https://github.com/frida/frida-java-bridge/blob/58030ace413a9104b8bf67f7396b22bf5d889e43/lib/android.js#L586
#ifdef __LP64__
        constexpr auto start_offset = 48;
#else
        constexpr auto start_offset = 50;
#endif
        constexpr auto end_offset = start_offset + 100;
        constexpr auto std_string_size = 3;
        auto sdk_int = GetAndroidApiLevel();

        bool success = true;

        auto instance = *(Runtime**) art.getSymbAddress("_ZN3art7Runtime9instance_E");
        if (instance == nullptr) {
            LOGE("not found: art::Runtime::instance_");
            return false;
        }
        LOGD("instance %p", instance);
        instance_ = instance;

        get_runtime_callbacks_ = reinterpret_cast<decltype(get_runtime_callbacks_)>(
                art.getSymbAddress("_ZN3art7Runtime19GetRuntimeCallbacksEv"));
        if (get_runtime_callbacks_ == nullptr) {
            LOGE("not found: Runtime::GetRuntimeCallbacks");
            success = false;
        }

        add_class_load_callback_ = reinterpret_cast<decltype(add_class_load_callback_)>(
                art.getSymbAddress("_ZN3art16RuntimeCallbacks20AddClassLoadCallbackEPNS_17ClassLoadCallbackE"));
        if (add_class_load_callback_ == nullptr) {
            LOGE("not found: RuntimeCallbacks::AddClassLoadCallback");
            success = false;
        }

        remove_class_load_callback_ = reinterpret_cast<decltype(remove_class_load_callback_)>(
                art.getSymbAddress("_ZN3art16RuntimeCallbacks23RemoveClassLoadCallbackEPNS_17ClassLoadCallbackE"));
        if (remove_class_load_callback_ == nullptr) {
            LOGE("not found: RuntimeCallbacks::RemoveClassLoadCallback");
            success = false;
        }

        // get classLinker

        JavaVM *vm;
        env->GetJavaVM(&vm);
        if (vm) {
            auto class_linker_offset = 0u;
            for (auto offset = start_offset; offset != end_offset; offset++) {
                if (*((void **) instance + offset) == vm) {
                    if (sdk_int >= __ANDROID_API_T__) {
                        class_linker_offset = offset - 4;
                    } else if (sdk_int >= __ANDROID_API_R__) {
                        auto try_class_linker = [&](int class_linker_offset) {
                            auto intern_table_offset = class_linker_offset - 1;
                            constexpr auto start_offset = 25;
                            constexpr auto end_offset = start_offset + 100;
                            auto class_linker = *(void **) ((void **) instance +
                                                            class_linker_offset);
                            auto intern_table = *(void **) ((void **) instance +
                                                            intern_table_offset);
                            if (!is_pointer_valid(class_linker)) return false;
                            for (auto i = start_offset; i != end_offset; i++) {
                                auto p = (void **) class_linker + i;
                                if (is_pointer_valid(p) && (*p) == intern_table) return true;
                            }
                            return false;
                        };
                        if (try_class_linker(offset - 3)) {
                            class_linker_offset = offset - 3;
                        } else if (try_class_linker(offset - 4)) {
                            class_linker_offset = offset - 4;
                        } else {
                            success = false;
                            break;
                        }
                    } else if (sdk_int >= __ANDROID_API_Q__) {
                        class_linker_offset = offset - 2;
                    } else if (sdk_int >= __ANDROID_API_O_MR1__) {
                        class_linker_offset = offset - std_string_size - 3;
                    } else {
                        class_linker_offset = offset - std_string_size - 2;
                    }
                }
            }
            if (class_linker_offset == 0u) {
                success = false;
            }
            class_linker_offset_ = class_linker_offset;
            LOGD("class_linker_offset_ %u", class_linker_offset);
            success &= ClassLinker::Init(art);
        } else {
            success = false;
        }

        // debuggable

        if (auto fn = art.getSymbAddress(
                    "_ZN3art7Runtime20SetRuntimeDebugStateENS0_17RuntimeDebugStateE"); fn) {
            static constexpr size_t kLargeEnoughSizeForRuntime = 4096;
            std::array<uint8_t, kLargeEnoughSizeForRuntime> code{};
            static_assert(static_cast<int>(RuntimeDebugState::kJavaDebuggable) != 0);
            static_assert(static_cast<int>(RuntimeDebugState::kJavaDebuggableAtInit) != 0);
            code.fill(uint8_t{0});
            auto *const fake_runtime = reinterpret_cast<Runtime *>(code.data());
            reinterpret_cast<void (*)(Runtime *, RuntimeDebugState)>(fn)(fake_runtime,
                                                                         RuntimeDebugState::kJavaDebuggable);
            for (size_t i = 0; i < kLargeEnoughSizeForRuntime; ++i) {
                if (*reinterpret_cast<RuntimeDebugState *>(
                        reinterpret_cast<uintptr_t>(fake_runtime) + i) ==
                    RuntimeDebugState::kJavaDebuggable) {
                    LOGD("found debug_state at offset %zu", i);
                    debug_state_offset = i;
                    break;
                }
            }
        }

        if (auto fn = art.getSymbAddress("_ZN3art7Runtime17SetJavaDebuggableEb"); fn) {
            static constexpr size_t kLargeEnoughSizeForRuntime = 4096;
            std::array<uint8_t, kLargeEnoughSizeForRuntime> code{};
            static_assert(static_cast<int>(RuntimeDebugState::kJavaDebuggable) != 0);
            static_assert(static_cast<int>(RuntimeDebugState::kJavaDebuggableAtInit) != 0);
            code.fill(uint8_t{0});
            auto *const fake_runtime = reinterpret_cast<Runtime *>(code.data());
            reinterpret_cast<void (*)(Runtime *, bool)>(fn)(fake_runtime, true);
            for (size_t i = 0; i < kLargeEnoughSizeForRuntime; ++i) {
                if (*reinterpret_cast<bool *>(
                        reinterpret_cast<uintptr_t>(fake_runtime) + i)) {
                    LOGD("found java_debuggable at offset %zu", i);
                    java_debuggable_offset = i;
                    break;
                }
            }
        }

        if (java_debuggable_offset == -1 && debug_state_offset == -1) {
            LOGE("not java_debuggable_offset nor debug_state_offset found");
            success = false;
        }

        success |= Thread::Init(art) && ReaderWriterMutex::Init(art);
        classlinker_class_lock_ptr = reinterpret_cast<decltype(classlinker_class_lock_ptr)>(
            art.getSymbAddress("_ZN3art5Locks25classlinker_classes_lock_E")
            );
        success |= classlinker_class_lock_ptr != nullptr;

        return success;
    }

    RuntimeCallbacks *Runtime::GetRuntimeCallbacks() {
        return get_runtime_callbacks_(this);
    }

    void RuntimeCallbacks::AddClassLoadCallback(art::ClassLoadCallback *cb) {
        add_class_load_callback_(this, cb);
    }

    void RuntimeCallbacks::RemoveClassLoadCallback(art::ClassLoadCallback *cb) {
        remove_class_load_callback_(this, cb);
    }

    ClassLinker* Runtime::getClassLinker() {
        return *((ClassLinker**)this + class_linker_offset_);
    }

    int SetDebuggableValue(int value) {
        if (debug_state_offset != -1) {
            auto addr = reinterpret_cast<RuntimeDebugState*>(reinterpret_cast<uintptr_t>(Runtime::Current()) + debug_state_offset);
            int orig = static_cast<int>(*addr);
            *addr = static_cast<RuntimeDebugState>(value);
            return orig;
        }

        if (java_debuggable_offset != -1) {
            auto addr = reinterpret_cast<bool*>(reinterpret_cast<uintptr_t>(Runtime::Current()) + java_debuggable_offset);
            int orig = *addr;
            *addr = true;
            return orig;
        }

        return 0;
    }

    int SetDebuggable(bool allow) {
        if (debug_state_offset != -1) {
            return SetDebuggableValue(static_cast<int>(allow ? RuntimeDebugState::kJavaDebuggable : RuntimeDebugState::kNonJavaDebuggable));
        } else if (java_debuggable_offset != -1) {
            return SetDebuggableValue(allow ? 1 : 0);
        }
        return 0;
    }

    void (*symSetJdwpAllowed)(bool) = nullptr;
    bool (*symIsJdwpAllowed)() = nullptr;

    bool Init(JNIEnv *env, elf_parser::Elf &art) {
        bool success = true;

        symSetJdwpAllowed = reinterpret_cast<decltype(symSetJdwpAllowed)>(art.getSymbAddress("_ZN3art3Dbg14SetJdwpAllowedEb"));
        if (!symSetJdwpAllowed) {
            LOGE("not found: art::Dbg::SetJdwpAllowed");
            success = false;
        }

        symIsJdwpAllowed = reinterpret_cast<decltype(symIsJdwpAllowed)>(art.getSymbAddress("_ZN3art3Dbg13IsJdwpAllowedEv"));
        if (!symIsJdwpAllowed) {
            LOGE("not found: art::Dbg::IsJdwpAllowed");
            success = false;
        }

        success &= Runtime::Init(env, art);

        return success;
    }

    inline void SetJdwpAllowed(bool allow) {
        symSetJdwpAllowed(allow);
    }

    inline bool IsJdwpAllowed() {
        return symIsJdwpAllowed();
    }

    Thread* (*current_fn_)() = nullptr;
    void (*artJniMethodStart)(Thread*) = nullptr;
    void (*artJniMethodEnd)(Thread*) = nullptr;
    bool Thread::Init(elf_parser::Elf& art) {
        current_fn_ = reinterpret_cast<decltype(current_fn_)>(art.getSymbAddress("_ZN3art6Thread14CurrentFromGdbEv"));
        if (!current_fn_) LOGE("_ZN3art6Thread14CurrentFromGdbEv not found");
        artJniMethodStart = reinterpret_cast<decltype(artJniMethodStart)>(
            art.getSymbAddress("artJniMethodStart"));
        if (!artJniMethodStart) LOGE("artJniMethodStart not found");
        artJniMethodEnd = reinterpret_cast<decltype(artJniMethodEnd)>(
            art.getSymbAddress("artJniMethodEnd"));
        if (!artJniMethodEnd) LOGE("artJniMethodEnd not found");
        return current_fn_ && artJniMethodStart && artJniMethodEnd;
    }

    Thread *Thread::Current() {
        return current_fn_();
    }

    void Thread::TransitionFromRunnableToSuspended(art::ThreadState new_state) {
        artJniMethodStart(this);
        SetState(new_state);
    }

    void Thread::TransitionFromSuspendedToRunnable() {
        artJniMethodEnd(this);
    }

    bool reader_writer_mutex_initialized = false;
    void (*reader_writer_mutex_ctor)(ReaderWriterMutex*, const char*, uint8_t) = nullptr;
    void (*reader_writer_mutex_dtor)(ReaderWriterMutex*) = nullptr;
    void (*reader_writer_mutex_HandleSharedLockContention)(ReaderWriterMutex*, Thread*, int32_t) = nullptr;
    void (*reader_writer_mutex_ExclusiveLock)(ReaderWriterMutex*, Thread*) = nullptr;
    void (*reader_writer_mutex_ExclusiveUnlock)(ReaderWriterMutex*, Thread*) = nullptr;
    size_t reader_writer_mutex_state_offset = -1;

    bool ReaderWriterMutex::Init(elf_parser::Elf &art) {
        reader_writer_mutex_ctor = reinterpret_cast<decltype(reader_writer_mutex_ctor)>
            (art.getSymbAddress("_ZN3art17ReaderWriterMutexC1EPKcNS_9LockLevelE"));
        if (!reader_writer_mutex_ctor) LOGE("_ZN3art17ReaderWriterMutexC1EPKcNS_9LockLevelE not found");
        reader_writer_mutex_dtor = reinterpret_cast<decltype(reader_writer_mutex_dtor)>
            (art.getSymbAddress("_ZN3art17ReaderWriterMutexD1Ev"));
        if (!reader_writer_mutex_dtor) LOGE("_ZN3art17ReaderWriterMutexD1Ev not found");
        reader_writer_mutex_HandleSharedLockContention = reinterpret_cast<
            decltype(reader_writer_mutex_HandleSharedLockContention)>
            (art.getSymbAddress("_ZN3art17ReaderWriterMutex26HandleSharedLockContentionEPNS_6ThreadEi"));
        if (!reader_writer_mutex_HandleSharedLockContention) LOGE("_ZN3art17ReaderWriterMutex26HandleSharedLockContentionEPNS_6ThreadEi not found");
        reader_writer_mutex_ExclusiveLock = reinterpret_cast<decltype(reader_writer_mutex_ExclusiveLock)>
            (art.getSymbAddress("_ZN3art17ReaderWriterMutex13ExclusiveLockEPNS_6ThreadE"));
        if (!reader_writer_mutex_ExclusiveLock) LOGE("_ZN3art17ReaderWriterMutex13ExclusiveLockEPNS_6ThreadE not found");
        reader_writer_mutex_ExclusiveUnlock = reinterpret_cast<decltype(reader_writer_mutex_ExclusiveUnlock)>
        (art.getSymbAddress("_ZN3art17ReaderWriterMutex15ExclusiveUnlockEPNS_6ThreadE"));
        if (!reader_writer_mutex_ExclusiveUnlock) LOGE("_ZN3art17ReaderWriterMutex15ExclusiveUnlockEPNS_6ThreadE not found");

        if (!reader_writer_mutex_ctor || !reader_writer_mutex_dtor || !reader_writer_mutex_ExclusiveLock
        || !reader_writer_mutex_ExclusiveUnlock || !reader_writer_mutex_HandleSharedLockContention) return false;

        std::array<uint8_t, 256> buf{};
        std::fill(buf.begin(), buf.end(), 0u);

        auto ptr = reinterpret_cast<ReaderWriterMutex*>(buf.data());
        reader_writer_mutex_ctor(ptr, "", 0);

        reader_writer_mutex_ExclusiveLock(ptr, Thread::Current());
        for (auto i = 0; i < 256; i += alignof(int32_t)) {
            if (*reinterpret_cast<int32_t*>(buf.data() + i) == -1) {
                reader_writer_mutex_state_offset = i;
                LOGD("reader_writer_mutex_state_offset %zu", reader_writer_mutex_state_offset);
                break;
            }
        }
        reader_writer_mutex_ExclusiveUnlock(ptr, Thread::Current());

        reader_writer_mutex_dtor(ptr);

        return reader_writer_mutex_state_offset != -1;
    }

    // https://cs.android.com/android/platform/superproject/main/+/main:art/runtime/base/mutex-inl.h;l=187-233;drc=61197364367c9e404c7da6900658f1b16c42d0da

    void ReaderWriterMutex::SharedLock(art::Thread *self) {
        if (!reader_writer_mutex_initialized) [[unlikely]] return;
        bool done = false;
        auto &state_ = *reinterpret_cast<std::atomic<int32_t>*>(reinterpret_cast<uintptr_t>(this) + reader_writer_mutex_state_offset);
        do {
            int32_t cur_state = state_.load(std::memory_order_relaxed);
            if (cur_state >= 0) [[likely]] {
                // Add as an extra reader.
                done = state_.compare_exchange_weak(cur_state, cur_state + 1, std::memory_order_acquire);
            } else {
                reader_writer_mutex_HandleSharedLockContention(this, self, cur_state);
            }
        } while (!done);
    }

    static inline int futex(volatile int *uaddr, int op, int val, const struct timespec *timeout,
                            volatile int *uaddr2, int val3) {
        return syscall(SYS_futex, uaddr, op, val, timeout, uaddr2, val3);
    }

    void ReaderWriterMutex::SharedUnlock(art::Thread *self) {
        if (!reader_writer_mutex_initialized) [[unlikely]] return;

        bool done = false;
        auto &state_ = *reinterpret_cast<std::atomic<int32_t>*>(reinterpret_cast<uintptr_t>(this) + reader_writer_mutex_state_offset);
        do {
            int32_t cur_state = state_.load(std::memory_order_relaxed);
            if (cur_state > 0) [[likely]] {
                // Reduce state by 1 and impose lock release load/store ordering.
                // Note, the num_contenders_ load below musn't reorder before the CompareAndSet.
                done = state_.compare_exchange_weak(cur_state, cur_state - 1);
                if (done && (cur_state - 1) == 0) {  // Weak CAS may fail spuriously.
                    // if (num_contenders_.load(std::memory_order_seq_cst) > 0) {
                    // Wake any exclusive waiters as there are now no readers.
                    futex((int*) &state_, FUTEX_WAKE_PRIVATE, INT_MAX, nullptr, nullptr, 0);
                    // }
                }
            } else {
                // LOG(FATAL) << "Unexpected state_:" << cur_state << " for " << name_;
            }
        } while (!done);
    }

    ReaderWriterMutex* classlinker_classes_lock() {
        return classlinker_class_lock_ptr ? *classlinker_class_lock_ptr : nullptr;
    }
}

extern "C"
JNIEXPORT jint JNICALL
Java_io_github_a13e300_tools_NativeUtils_nativeSetJavaDebug(JNIEnv *, jclass,
                                                            jboolean allow, jint orig) {
    if (allow == JNI_TRUE) {
        return art::SetDebuggable(true);
    } else {
        return art::SetDebuggableValue(orig);
    }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_io_github_a13e300_tools_NativeUtils_nativeSetJdwp(JNIEnv *env, jclass clazz, jboolean allow, jboolean orig) {
    auto o = art::IsJdwpAllowed();
    art::SetJdwpAllowed(allow == JNI_TRUE || orig == JNI_TRUE);
    return o;
}
