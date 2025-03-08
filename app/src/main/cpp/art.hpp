#pragma once
#include "elf_parser.hpp"

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

namespace art {
    namespace mirror {

        class Object {

        };

        class MANAGED ClassLoader : public Object {
        };

        class MANAGED Class : public Object {
        };

        template<class MirrorType>
        class ObjPtr {
            uintptr_t reference_;

        public:
            MirrorType *Ptr() const {
                return reinterpret_cast<MirrorType*>(reference_);
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
        };
    }

    template<class MirrorType>
    class PACKED(4) StackReference : public mirror::CompressedReference<MirrorType> {
    };

    class ValueObject {
    };

    template<class T>
    class Handle : public ValueObject {
    public:
        constexpr Handle() : reference_(nullptr) {
        }

        constexpr ALWAYS_INLINE Handle(const Handle<T>& handle) = default;

        ALWAYS_INLINE Handle<T>& operator=(const Handle<T>& handle) = default;

        ALWAYS_INLINE T& operator*() const REQUIRES_SHARED(Locks::mutator_lock_) {
            return *Get();
        }

        ALWAYS_INLINE T* operator->() const REQUIRES_SHARED(Locks::mutator_lock_) {
            return Get();
        }

        ALWAYS_INLINE T* Get() const REQUIRES_SHARED(Locks::mutator_lock_) {
            return static_cast<T*>(reference_->AsMirrorPtr());
        }

        ALWAYS_INLINE bool IsNull() const {
            // It's safe to null-check it without a read barrier.
            return reference_->IsNull();
        }

        ALWAYS_INLINE bool operator!=(std::nullptr_t) const {
            return !IsNull();
        }

        ALWAYS_INLINE bool operator==(std::nullptr_t) const {
            return IsNull();
        }

    protected:

        StackReference<mirror::Object>* reference_;

    private:
        friend class BuildGenericJniFrameVisitor;
        template<class S> friend class Handle;
        friend class HandleScope;
        template<class S> friend class HandleWrapper;
        template<size_t kNumReferences> friend class StackHandleScope;
    };

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

        virtual void
        VisitRoots(mirror::CompressedReference<mirror::Object> **roots, size_t count,
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

    class ClassVisitor {
    public:
        virtual ~ClassVisitor() {};
        // Return true to continue visiting.
        virtual bool operator()(mirror::ObjPtr<mirror::Class> klass) = 0;
    };

    class ClassLinker {
    private:
        static void (*visit_class_loader_)(void *, ClassLoaderVisitor *);
        static void (*visit_classes_)(void*, ClassVisitor*);

    public:
        static bool Init(elf_parser::Elf &art);

        void VisitClassLoaders(ClassLoaderVisitor *clv);
        void VisitClasses(ClassVisitor *visitor);
    };


    class ClassLoadCallback {
    public:
        virtual ~ClassLoadCallback() {}

        // Called immediately before beginning class-definition and immediately before returning from it.
        virtual void BeginDefineClass() REQUIRES_SHARED(Locks::mutator_lock_) {}
        virtual void EndDefineClass() REQUIRES_SHARED(Locks::mutator_lock_) {}

        // If set we will replace initial_class_def & initial_dex_file with the final versions. The
        // callback author is responsible for ensuring these are allocated in such a way they can be
        // cleaned up if another transformation occurs. Note that both must be set or null/unchanged on
        // return.
        // Note: the class may be temporary, in which case a following ClassPrepare event will be a
        //       different object. It is the listener's responsibility to handle this.
        // Note: This callback is rarely useful so a default implementation has been given that does
        //       nothing.
        virtual void ClassPreDefine([[maybe_unused]] const char* descriptor,
                                    [[maybe_unused]] Handle<mirror::Class> klass,
                                    [[maybe_unused]] Handle<mirror::ClassLoader> class_loader,
                                    [[maybe_unused]] void* /*const DexFile&*/ initial_dex_file,
                                    [[maybe_unused]] void* /*const dex::ClassDef&*/ initial_class_def,
                                    [[maybe_unused]] /*out*/ void* /*DexFile const***/ final_dex_file,
                                    [[maybe_unused]] /*out*/ void* /*dex::ClassDef const** */ final_class_def)
        REQUIRES_SHARED(Locks::mutator_lock_) {}

        // A class has been loaded.
        // Note: the class may be temporary, in which case a following ClassPrepare event will be a
        //       different object. It is the listener's responsibility to handle this.
        virtual void ClassLoad(Handle<mirror::Class> klass) REQUIRES_SHARED(Locks::mutator_lock_) = 0;

        // A class has been prepared, i.e., resolved. As the ClassLoad event might have been for a
        // temporary class, provide both the former and the current class.
        virtual void ClassPrepare(Handle<mirror::Class> temp_klass,
                                  Handle<mirror::Class> klass) REQUIRES_SHARED(Locks::mutator_lock_) = 0;
    };

    class RuntimeCallbacks {
    public:
        void AddClassLoadCallback(ClassLoadCallback* cb);
        void RemoveClassLoadCallback(ClassLoadCallback* cb);
    };

    class Runtime {
    private:
        static Runtime *instance_;
    public:
        static bool Init(JNIEnv *env, elf_parser::Elf &art);

        ClassLinker* getClassLinker();
        inline static Runtime *Current() { return instance_; }
        RuntimeCallbacks* GetRuntimeCallbacks();
    };

    bool Init(JNIEnv *env, elf_parser::Elf &art);
}
