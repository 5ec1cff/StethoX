#pragma once
#include "elf_parser.hpp"
#include <atomic>

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

    template<class MirrorType>
    class ObjPtr {
        uintptr_t reference_;

    public:
        MirrorType *Ptr() const {
            return reinterpret_cast<MirrorType*>(reference_);
        }
    };

    namespace mirror {

        class Object {

        };

        class MANAGED ClassLoader : public Object {
        };

        class MANAGED Class : public Object {
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

        virtual void Visit(ObjPtr<mirror::ClassLoader> class_loader)
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
        virtual bool operator()(ObjPtr<mirror::Class> klass) = 0;
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

    enum class CASMode {
        kStrong,
        kWeak,
    };

    template<typename T>
    class PACKED(sizeof(T)) Atomic : public std::atomic<T> {
    public:
        Atomic<T>() : std::atomic<T>(T()) { }

        explicit Atomic<T>(T value) : std::atomic<T>(value) { }

        // Load data from an atomic variable with Java data memory order semantics.
        //
        // Promises memory access semantics of ordinary Java data.
        // Does not order other memory accesses.
        // Long and double accesses may be performed 32 bits at a time.
        // There are no "cache coherence" guarantees; e.g. loads from the same location may be reordered.
        // In contrast to normal C++ accesses, racing accesses are allowed.
        T LoadJavaData() const {
            return this->load(std::memory_order_relaxed);
        }

        // Store data in an atomic variable with Java data memory ordering semantics.
        //
        // Promises memory access semantics of ordinary Java data.
        // Does not order other memory accesses.
        // Long and double accesses may be performed 32 bits at a time.
        // There are no "cache coherence" guarantees; e.g. loads from the same location may be reordered.
        // In contrast to normal C++ accesses, racing accesses are allowed.
        void StoreJavaData(T desired_value) {
            this->store(desired_value, std::memory_order_relaxed);
        }

        // Atomically replace the value with desired_value if it matches the expected_value.
        // Participates in total ordering of atomic operations.
        bool CompareAndSetStrongSequentiallyConsistent(T expected_value, T desired_value) {
            return this->compare_exchange_strong(expected_value, desired_value, std::memory_order_seq_cst);
        }

        // The same, except it may fail spuriously.
        bool CompareAndSetWeakSequentiallyConsistent(T expected_value, T desired_value) {
            return this->compare_exchange_weak(expected_value, desired_value, std::memory_order_seq_cst);
        }

        // Atomically replace the value with desired_value if it matches the expected_value. Doesn't
        // imply ordering or synchronization constraints.
        bool CompareAndSetStrongRelaxed(T expected_value, T desired_value) {
            return this->compare_exchange_strong(expected_value, desired_value, std::memory_order_relaxed);
        }

        // Atomically replace the value with desired_value if it matches the expected_value. Prior writes
        // to other memory locations become visible to the threads that do a consume or an acquire on the
        // same location.
        bool CompareAndSetStrongRelease(T expected_value, T desired_value) {
            return this->compare_exchange_strong(expected_value, desired_value, std::memory_order_release);
        }

        // The same, except it may fail spuriously.
        bool CompareAndSetWeakRelaxed(T expected_value, T desired_value) {
            return this->compare_exchange_weak(expected_value, desired_value, std::memory_order_relaxed);
        }

        // Atomically replace the value with desired_value if it matches the expected_value. Prior writes
        // made to other memory locations by the thread that did the release become visible in this
        // thread.
        bool CompareAndSetWeakAcquire(T expected_value, T desired_value) {
            return this->compare_exchange_weak(expected_value, desired_value, std::memory_order_acquire);
        }

        // Atomically replace the value with desired_value if it matches the expected_value. Prior writes
        // to other memory locations become visible to the threads that do a consume or an acquire on the
        // same location.
        bool CompareAndSetWeakRelease(T expected_value, T desired_value) {
            return this->compare_exchange_weak(expected_value, desired_value, std::memory_order_release);
        }

        // Atomically replace the value with desired_value if it matches the expected_value.
        // Participates in total ordering of atomic operations.
        // Returns the existing value before the exchange. In other words, if the returned value is the
        // same as expected_value, as passed to this method, the exchange has completed successfully.
        // Otherwise the value was left unchanged.
        T CompareAndExchangeStrongSequentiallyConsistent(T expected_value, T desired_value) {
            // compare_exchange_strong() modifies expected_value if the actual value found is different from
            // what was expected. In other words expected_value is changed if compare_exchange_strong
            // returns false.
            this->compare_exchange_strong(expected_value, desired_value, std::memory_order_seq_cst);
            return expected_value;
        }

        bool CompareAndSet(T expected_value,
                           T desired_value,
                           CASMode mode,
                           std::memory_order memory_order) {
            return mode == CASMode::kStrong
                   ? this->compare_exchange_strong(expected_value, desired_value, memory_order)
                   : this->compare_exchange_weak(expected_value, desired_value, memory_order);
        }

        // Returns the address of the current atomic variable. This is only used by futex() which is
        // declared to take a volatile address (see base/mutex-inl.h).
        volatile T* Address() {
            return reinterpret_cast<T*>(this);
        }

        static T MaxValue() {
            return std::numeric_limits<T>::max();
        }
    };

    enum class ThreadState : uint8_t {
        kTerminated = 66,                 // TERMINATED     TS_ZOMBIE    Thread.run has returned, but Thread* still around
        kRunnable = 0,                    // RUNNABLE       TS_RUNNING   runnable
        kObsoleteRunnable = 67,           // ---            ---          obsolete value
        kTimedWaiting = 68,               // TIMED_WAITING  TS_WAIT      in Object.wait() with a timeout
    };

    class StateAndFlags {
    public:
        explicit StateAndFlags(uint32_t value) :value_(value) {}

        uint32_t GetValue() const {
            return value_;
        }

        void SetValue(uint32_t value) {
            value_ = value;
        }

        ThreadState GetState() const {
            ThreadState state = static_cast<ThreadState>((value_ >> 24) & 0xff);
            return state;
        }

        void SetState(ThreadState state) {
            value_ = (static_cast<uint32_t>(state) << 24) | (value_ & ~0xff);
        }

        StateAndFlags WithState(ThreadState state) const {
            StateAndFlags result = *this;
            result.SetState(state);
            return result;
        }

        // The value holds thread flags and thread state.
        uint32_t value_;
    };

    class Thread {
    public:
        static Thread* Current();
        static bool Init(elf_parser::Elf& art);

        Atomic<uint32_t> state_and_flags;

        void TransitionFromSuspendedToRunnable();

        void TransitionFromRunnableToSuspended(ThreadState new_state);

        inline StateAndFlags GetStateAndFlags(std::memory_order order) const {
            return StateAndFlags(state_and_flags.load(order));
        }

        inline ThreadState GetState() const {
            return GetStateAndFlags(std::memory_order_relaxed).GetState();
        }

        inline ThreadState SetState(ThreadState new_state) {
            while (true) {
                StateAndFlags old_state_and_flags = GetStateAndFlags(std::memory_order_relaxed);
                StateAndFlags new_state_and_flags = old_state_and_flags.WithState(new_state);
                bool done =
                    state_and_flags.CompareAndSetWeakRelaxed(old_state_and_flags.GetValue(),
                                                             new_state_and_flags.GetValue());
                if (done) {
                    return static_cast<ThreadState>(old_state_and_flags.GetState());
                }
            }
        }
    };

    class ReaderWriterMutex {
    public:
        static bool Init(elf_parser::Elf& art);
        void SharedLock(Thread* self);
        void SharedUnlock(Thread* self);
    };

    class ReaderMutexLock {
        ReaderWriterMutex* mu_;
        Thread* self_;

    public:
        ReaderMutexLock(ReaderWriterMutex* mu): mu_(mu), self_(Thread::Current()) {
            if (mu_) mu_->SharedLock(self_);
        }

        ~ReaderMutexLock() {
            if (mu_) mu_->SharedUnlock(self_);
        }
    };

    ReaderWriterMutex* classlinker_classes_lock();

    class ScopedThreadStateChange {
        Thread* self_ = nullptr;
        const ThreadState thread_state_ = ThreadState::kTerminated;
        ThreadState old_thread_state_ = ThreadState::kTerminated;
        const bool expected_has_no_thread_ = true;
    public:
        ALWAYS_INLINE ScopedThreadStateChange(Thread* self, ThreadState new_thread_state): self_(self), thread_state_(new_thread_state), expected_has_no_thread_(false)  {
            old_thread_state_ = self->GetState();
            if (old_thread_state_ != new_thread_state) {
                if (new_thread_state == ThreadState::kRunnable) {
                    self_->TransitionFromSuspendedToRunnable();
                } else if (old_thread_state_ == ThreadState::kRunnable) {
                    self_->TransitionFromRunnableToSuspended(new_thread_state);
                } else {
                    // A transition between suspended states.
                    self_->SetState(new_thread_state);
                }
            }
        }

        ALWAYS_INLINE ~ScopedThreadStateChange() {
            if (old_thread_state_ != thread_state_) {
                if (old_thread_state_ == ThreadState::kRunnable) {
                    self_->TransitionFromSuspendedToRunnable();
                } else if (thread_state_ == ThreadState::kRunnable) {
                    self_->TransitionFromRunnableToSuspended(old_thread_state_);
                } else {
                    // A transition between suspended states.
                    self_->SetState(old_thread_state_);
                }
            }
        }

        ALWAYS_INLINE Thread* Self() const {
            return self_;
        }
    };

    class ScopedObjectAccess : ScopedThreadStateChange {
    public:
        ALWAYS_INLINE ScopedObjectAccess() : ScopedThreadStateChange(Thread::Current(), ThreadState::kRunnable) {}

        ~ScopedObjectAccess() = default;
    };
}
