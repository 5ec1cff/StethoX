#pragma once

#include <stdint.h>

#ifndef NDEBUG
#define ALWAYS_INLINE
#else
#define ALWAYS_INLINE  __attribute__ ((always_inline))
#endif

#define DCHECK(...)

#define OVERRIDE override

#define ATTRIBUTE_UNUSED __attribute__((__unused__))

#define REQUIRES_SHARED(...)

#define MANAGED PACKED(4)
#define PACKED(x) __attribute__ ((__aligned__(x), __packed__))

#include "utils.h"
#include <string>
#include "logging.h"
#include "plt.h"

namespace art {

    namespace mirror {

        class Object {

        };

        class MANAGED ClassLoader : public Object {};

        template<class MirrorType>
        class ObjPtr {

        public:
            MirrorType *Ptr() const {
                return nullptr;
            }

        };

        template<bool kPoisonReferences, class MirrorType>
        class PtrCompression {
        public:
            // Compress reference to its bit representation.
            static uint32_t Compress(MirrorType *mirror_ptr) {
                uintptr_t as_bits = reinterpret_cast<uintptr_t>(mirror_ptr);
                return static_cast<uint32_t>(kPoisonReferences ? -as_bits : as_bits);
            }

            // Uncompress an encoded reference from its bit representation.
            static MirrorType *Decompress(uint32_t ref) {
                uintptr_t as_bits = kPoisonReferences ? -ref : ref;
                return reinterpret_cast<MirrorType *>(as_bits);
            }

            // Convert an ObjPtr to a compressed reference.
            static uint32_t Compress(ObjPtr<MirrorType> ptr) REQUIRES_SHARED(Locks::mutator_lock_) {
                return Compress(ptr.Ptr());
            }
        };


        // Value type representing a reference to a mirror::Object of type MirrorType.
        template<bool kPoisonReferences, class MirrorType>
        class MANAGED ObjectReference {
        private:
            using Compression = PtrCompression<kPoisonReferences, MirrorType>;

        public:
            MirrorType *AsMirrorPtr() const {
                return Compression::Decompress(reference_);
            }

            void Assign(MirrorType *other) {
                reference_ = Compression::Compress(other);
            }

            void Assign(ObjPtr<MirrorType> ptr) REQUIRES_SHARED(Locks::mutator_lock_);

            void Clear() {
                reference_ = 0;
                DCHECK(IsNull());
            }

            bool IsNull() const {
                return reference_ == 0;
            }

            uint32_t AsVRegValue() const {
                return reference_;
            }

            static ObjectReference<kPoisonReferences, MirrorType>
            FromMirrorPtr(MirrorType *mirror_ptr)
            REQUIRES_SHARED(Locks::mutator_lock_) {
                return ObjectReference<kPoisonReferences, MirrorType>(mirror_ptr);
            }

        protected:
            explicit ObjectReference(MirrorType *mirror_ptr) REQUIRES_SHARED(Locks::mutator_lock_)
                    : reference_(Compression::Compress(mirror_ptr)) {
            }

            // The encoded reference to a mirror::Object.
            uint32_t reference_;
        };

        // Standard compressed reference used in the runtime. Used for StackReference and GC roots.
        template<class MirrorType>
        class MANAGED CompressedReference : public mirror::ObjectReference<false, MirrorType> {
        public:
            CompressedReference<MirrorType>() REQUIRES_SHARED(Locks::mutator_lock_)
                    : mirror::ObjectReference<false, MirrorType>(nullptr) {}

            static CompressedReference<MirrorType> FromMirrorPtr(MirrorType *p)
            REQUIRES_SHARED(Locks::mutator_lock_) {
                return CompressedReference<MirrorType>(p);
            }

        private:
            explicit CompressedReference(MirrorType *p) REQUIRES_SHARED(Locks::mutator_lock_)
                    : mirror::ObjectReference<false, MirrorType>(p) {}
        };
    }


    class ClassLoaderVisitor {
    public:
        virtual ~ClassLoaderVisitor() {}
        virtual void Visit(mirror::ClassLoader *class_loader)
        REQUIRES_SHARED(Locks::classlinker_classes_lock_, Locks::mutator_lock_) = 0;
    };

    class RootInfo {

    };

    class RootVisitor {
    public:
        virtual ~RootVisitor() {}

        // Single root version, not overridable.
        ALWAYS_INLINE void VisitRoot(mirror::Object **root, const RootInfo &info)
        REQUIRES_SHARED(Locks::mutator_lock_) {
            VisitRoots(&root, 1, info);
        }

        // Single root version, not overridable.
        ALWAYS_INLINE void VisitRootIfNonNull(mirror::Object **root, const RootInfo &info)
        REQUIRES_SHARED(Locks::mutator_lock_) {
            if (*root != nullptr) {
                VisitRoot(root, info);
            }
        }

        virtual void VisitRoots(mirror::Object ***roots, size_t count, const RootInfo &info)
        REQUIRES_SHARED(Locks::mutator_lock_) = 0;

        virtual void VisitRoots(mirror::CompressedReference<mirror::Object> **roots, size_t count,
                                const RootInfo &info)
        REQUIRES_SHARED(Locks::mutator_lock_) = 0;
    };

    // Only visits roots one at a time, doesn't handle updating roots. Used when performance isn't
    // critical.
    class SingleRootVisitor : public RootVisitor {
    private:
        void VisitRoots(mirror::Object ***roots, size_t count, const RootInfo &info) OVERRIDE
        REQUIRES_SHARED(Locks::mutator_lock_) {
            for (size_t i = 0; i < count; ++i) {
                VisitRoot(*roots[i], info);
            }
        }

        void VisitRoots(mirror::CompressedReference<mirror::Object> **roots, size_t count,
                        const RootInfo &info) OVERRIDE
        REQUIRES_SHARED(Locks::mutator_lock_) {
            for (size_t i = 0; i < count; ++i) {
                VisitRoot(roots[i]->AsMirrorPtr(), info);
            }
        }

        virtual void VisitRoot(mirror::Object *root, const RootInfo &info) = 0;
    };

    class IsMarkedVisitor {
    public:
        virtual ~IsMarkedVisitor() {}

        // Return null if an object is not marked, otherwise returns the new address of that object.
        // May return the same address as the input if the object did not move.
        virtual mirror::Object *IsMarked(mirror::Object *obj) = 0;
    };

    class ClassLinker {
    private:
        static inline void(*visit_class_loader_)(void*, ClassLoaderVisitor*);
    public:
        static bool Init() {
            auto VisitClassLoaders = (void(*)(void*, ClassLoaderVisitor*)) plt_dlsym("_ZNK3art11ClassLinker17VisitClassLoadersEPNS_18ClassLoaderVisitorE",
                                                                                     nullptr);
            LOGD("VisitClassLoaders: %p", VisitClassLoaders);
            if (VisitClassLoaders == nullptr) return false;
            visit_class_loader_ = VisitClassLoaders;
            return true;
        }

        inline void VisitClassLoaders(ClassLoaderVisitor* clv) {
            if (visit_class_loader_ != nullptr) {
                visit_class_loader_(this, clv);
            }
        }
    };

    class Runtime {
    private:
        inline static Runtime *instance_;
        inline static unsigned int class_linker_offset_;
    public:
        static bool Init(JNIEnv *env) {
            // https://github.com/frida/frida-java-bridge/blob/58030ace413a9104b8bf67f7396b22bf5d889e43/lib/android.js#L586
#ifdef __LP64__
            constexpr auto start_offset = 48;
#else
            constexpr auto start_offset = 50;
#endif
            constexpr auto end_offset = start_offset + 100;
            constexpr auto std_string_size = 3;
            auto sdk_int = GetAndroidApiLevel();
            auto instance = *(Runtime**)plt_dlsym("_ZN3art7Runtime9instance_E", nullptr);
            if (instance == nullptr) return false;
            LOGD("instance %p", instance);
            instance_ = instance;
            JavaVM *vm;
            env->GetJavaVM(&vm);
            if (vm == nullptr) return false;
            auto class_linker_offset = 0u;
            for (auto offset = start_offset; offset != end_offset; offset ++) {
                if (*((void**)instance + offset) == vm) {
                    if (sdk_int >= __ANDROID_API_T__) {
                        class_linker_offset = offset - 4;
                    } else if (sdk_int >= __ANDROID_API_R__) {
                        auto try_class_linker = [&](int class_linker_offset) {
                            auto intern_table_offset = class_linker_offset - 1;
                            constexpr auto start_offset = 25;
                            constexpr auto end_offset = start_offset + 100;
                            auto class_linker = *(void**)((void**)instance + class_linker_offset);
                            auto intern_table = *(void**)((void**)instance + intern_table_offset);
                            if (!is_pointer_valid(class_linker)) return false;
                            for (auto i = start_offset; i != end_offset; i++) {
                                auto p = (void**) class_linker + i;
                                if (is_pointer_valid(p) && (*p) == intern_table) return true;
                            }
                            return false;
                        };
                        if (try_class_linker(offset - 3)) {
                            class_linker_offset = offset - 3;
                        } else if (try_class_linker(offset - 4)) {
                            class_linker_offset = offset - 4;
                        } else {
                            return false;
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
            if (class_linker_offset == 0u) return false;
            class_linker_offset_ = class_linker_offset;
            LOGD("class_linker_offset_ %u", class_linker_offset);
            return ClassLinker::Init();
        }

        ClassLinker* getClassLinker() {
            return *((ClassLinker**)this + class_linker_offset_);
        }

        inline static Runtime *Current() { return instance_; }
    };
}
