#include <jni.h>
#include <fcntl.h>
#include <unistd.h>
#include "classloader.h"
#include "logging.h"
#include "art.hpp"
#include <functional>
#include <utility>
#include <vector>
#include <memory>

struct JNIEnvExt {
private:
    static inline jobject (*symNewLocalRef)(JNIEnvExt *, art::mirror::Object *) = nullptr;
    static inline void (*symDeleteLocalRef)(JNIEnvExt *, jobject) = nullptr;
public:
    static bool Init(elf_parser::Elf &art) {
        bool success = true;
        symNewLocalRef = reinterpret_cast<decltype(symNewLocalRef)>(art.getSymbAddress("_ZN3art9JNIEnvExt11NewLocalRefEPNS_6mirror6ObjectE"));
        if (!symNewLocalRef) {
            success = false;
            LOGE("not found: art::JNIEnvExt::NewLocalRef");
        }
        symDeleteLocalRef = reinterpret_cast<decltype(symDeleteLocalRef)>(art.getSymbAddress("_ZN3art9JNIEnvExt14DeleteLocalRefEP8_jobject"));
        if (!symDeleteLocalRef) {
            success = false;
            LOGE("not found: art::JNIEnvExt::DeleteLocalRef");
        }
        return success;
    }

    inline jobject NewLocalRef(art::mirror::Object* object) {
        return symNewLocalRef(this, object);
    }

    inline void DeleteLocalRef(jobject object) {
        symDeleteLocalRef(this, object);
    }

    static inline JNIEnvExt* From(JNIEnv* env) {
        return reinterpret_cast<JNIEnvExt*>(env);
    }
};

struct JavaVMExt {
private:
    static inline void (*symVisitRoots)(JavaVMExt*, art::RootVisitor*) = nullptr;
    static inline void (*symSweepJniWeakGlobals)(JavaVMExt*, art::IsMarkedVisitor*) = nullptr;

public:
    static bool Init(elf_parser::Elf &art) {
        bool success = true;
        symVisitRoots = reinterpret_cast<decltype(symVisitRoots)>(art.getSymbAddress("_ZN3art9JavaVMExt10VisitRootsEPNS_11RootVisitorE"));
        if (!symVisitRoots) {
            success = false;
            LOGE("not found: art::JavaVMExt::VisitRoots");
        }
        symSweepJniWeakGlobals = reinterpret_cast<decltype(symSweepJniWeakGlobals)>(art.getSymbAddress("_ZN3art9JavaVMExt19SweepJniWeakGlobalsEPNS_15IsMarkedVisitorE"));
        if (!symSweepJniWeakGlobals) {
            // success = false;
            LOGE("not found: art::JavaVMExt::SweepJniWeakGlobals");
        }
        return success;
    }

    inline void VisitRoots(art::RootVisitor* visitor) {
        if (symVisitRoots) symVisitRoots(this, visitor);
    }

    inline void SweepJniWeakGlobals(art::IsMarkedVisitor* visitor) {
        if (symSweepJniWeakGlobals) symSweepJniWeakGlobals(this, visitor);
    }

    static inline JavaVMExt* From(JavaVM* vm) {
        return reinterpret_cast<JavaVMExt*>(vm);
    }
};

using Callback = std::function<void(art::mirror::Object*)>;

class ClassLoaderVisitor : public art::SingleRootVisitor {
private:
    Callback callback_;
    jmethodID toStringMid;
public:
    ClassLoaderVisitor(JNIEnv *env, jclass classLoader, Callback callback) : env_(env), classLoader_(classLoader), callback_(std::move(callback)) {
        auto objectClass = env_->FindClass("java/lang/Class");
        toStringMid = env_->GetMethodID(objectClass, "getName", "()Ljava/lang/String;");
    }

    void VisitRoot(art::mirror::Object *root, const art::RootInfo &info ATTRIBUTE_UNUSED) final {
        jobject object = JNIEnvExt::From(env_)->NewLocalRef(root);
        if (object != nullptr) {
            auto s = (jstring) env_->CallObjectMethod(env_->GetObjectClass(object), toStringMid);
            auto c = env_->GetStringUTFChars(s, nullptr);
            env_->ReleaseStringUTFChars(s, c);
            LOGD("object name %s", c);
            if (env_->IsInstanceOf(object, classLoader_)) {
                callback_(root);
            }
            JNIEnvExt::From(env_)->DeleteLocalRef(object);
        }
    }

private:
    JNIEnv *env_;
    jclass classLoader_;
};

class MyClassLoaderVisitor : public art::ClassLoaderVisitor {
public:
    MyClassLoaderVisitor(JNIEnv *env, Callback callback) : env_(env), callback_(std::move(callback)) {
    }

    void Visit(art::mirror::ClassLoader *class_loader) override {
        callback_(class_loader);
    }

private:
    JNIEnv *env_;
    Callback callback_;
};

jobjectArray visitClassLoaders(JNIEnv *env) {
    jclass class_loader_class = env->FindClass("java/lang/ClassLoader");
    std::vector<jobject> class_loaders;
    auto callback = [&](art::mirror::Object* o) {
        auto r = JNIEnvExt::From(env)->NewLocalRef(reinterpret_cast<art::mirror::Object*>(o));
        class_loaders.push_back(r);
    };
    MyClassLoaderVisitor v(env, callback);
    art::Runtime::Current()->getClassLinker()->VisitClassLoaders(&v);
    auto arr = env->NewObjectArray(static_cast<jsize>(class_loaders.size()), class_loader_class, nullptr);
    for (auto i = 0; i < class_loaders.size(); i++) {
        auto o = class_loaders[i];
        env->SetObjectArrayElement(arr, i, o);
        JNIEnvExt::From(env)->DeleteLocalRef(o);
    }
    return arr;
}

class WeakClassLoaderVisitor : public art::IsMarkedVisitor {
    Callback callback_;
    jmethodID toStringMid;
public :
    WeakClassLoaderVisitor(JNIEnv *env, jclass classLoader, Callback callback) : env_(env), classLoader_(classLoader), callback_(std::move(callback)) {
        auto objectClass = env_->FindClass("java/lang/Class");
        toStringMid = env_->GetMethodID(objectClass, "getName", "()Ljava/lang/String;");
    }

    art::mirror::Object *IsMarked(art::mirror::Object *obj) override {
        jobject object = JNIEnvExt::From(env_)->NewLocalRef(obj);
        if (object != nullptr) {
            if (env_->IsInstanceOf(object, classLoader_)) {
                auto s = (jstring) env_->CallObjectMethod(env_->GetObjectClass(object), toStringMid);
                auto c = env_->GetStringUTFChars(s, nullptr);
                env_->ReleaseStringUTFChars(s, c);
                LOGD("object name %s", c);
                callback_(obj);
            }
            JNIEnvExt::From(env_)->DeleteLocalRef(object);
        }
        return obj;
    }

private:
    JNIEnv *env_;
    jclass classLoader_;
};

jobjectArray visitClassLoadersByRootVisitor(JNIEnv *env) {
    JavaVM *jvm;
    env->GetJavaVM(&jvm);
    jclass class_loader_class = env->FindClass("java/lang/ClassLoader");
    std::vector<jobject> class_loaders;
    auto callback = [&](art::mirror::Object* o) {
        class_loaders.push_back(JNIEnvExt::From(env)->NewLocalRef(o));
    };
    {
        ClassLoaderVisitor visitor(env, class_loader_class, callback);
        JavaVMExt::From(jvm)->VisitRoots(&visitor);
    }
    WeakClassLoaderVisitor visitor(env, class_loader_class, callback);
    JavaVMExt::From(jvm)->SweepJniWeakGlobals(&visitor);
    auto arr = env->NewObjectArray(class_loaders.size(), class_loader_class, nullptr);
    for (auto i = 0; i < class_loaders.size(); i++) {
        auto o = class_loaders[i];
        env->SetObjectArrayElement(arr, i, o);
        JNIEnvExt::From(env)->DeleteLocalRef(o);
    }
    return arr;
}

bool InitClassLoaders(elf_parser::Elf &art) {
    bool success = true;
    if (!JNIEnvExt::Init(art)) {
        LOGE("JNIEnvExt init failed");
        success = false;
    }
    if (!JavaVMExt::Init(art)) {
        LOGE("JavaVMExt init failed");
        success = false;
    }
    return success;
}

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_io_github_a13e300_tools_NativeUtils_getClassLoaders(JNIEnv *env, jclass clazz) {
    return visitClassLoaders(env);
}

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_io_github_a13e300_tools_NativeUtils_getClassLoaders2(JNIEnv *env, jclass clazz) {
    return visitClassLoadersByRootVisitor(env);
}

struct GlobalRefVisitor : public art::SingleRootVisitor {
private:
    Callback callback_;
public:
    explicit GlobalRefVisitor(Callback&& callback) : callback_(callback) {}

    void VisitRoot(art::mirror::Object *root, const art::RootInfo &info) final {
        callback_(root);
    }
};

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_io_github_a13e300_tools_NativeUtils_getGlobalRefs(JNIEnv *env, jclass, jclass clazz) {
    JavaVM *jvm;
    env->GetJavaVM(&jvm);

    std::vector<jobject> objects;
    GlobalRefVisitor visitor{[&](art::mirror::Object* o) {
        auto obj = JNIEnvExt::From(env)->NewLocalRef(o);
        if (!clazz || env->IsInstanceOf(obj, clazz))
            objects.push_back(obj);
        else
            JNIEnvExt::From(env)->DeleteLocalRef(obj);
    }};

    JavaVMExt::From(jvm)->VisitRoots(&visitor);

    if (clazz == nullptr) {
        clazz = env->FindClass("java/lang/Object");
    }

    auto arr = env->NewObjectArray(static_cast<jsize>(objects.size()), clazz, nullptr);
    for (auto i = 0; i < objects.size(); i++) {
        auto o = objects[i];
        env->SetObjectArrayElement(arr, i, o);
        JNIEnvExt::From(env)->DeleteLocalRef(o);
    }
    return arr;
}

extern "C"
JNIEXPORT void JNICALL
Java_io_github_a13e300_tools_NativeUtils_visitClasses(JNIEnv *env, jclass clazz) {
    class Visitor : public art::ClassVisitor {
        JNIEnv *env_;
        jmethodID getClassName;
        bool operator()(art::mirror::ObjPtr<art::mirror::Class> klass) override {
            auto ref = JNIEnvExt::From(env_)->NewLocalRef(klass.Ptr());
            auto str = (jstring) env_->CallObjectMethod(ref, getClassName);
            auto chars = env_->GetStringUTFChars(str, nullptr);
            LOGD("class %s", chars);
            env_->ReleaseStringUTFChars(str, chars);
            JNIEnvExt::From(env_)->DeleteLocalRef(ref);
            return true;
        }
    public :
        explicit Visitor(JNIEnv* env) : env_(env) {
            getClassName = env->GetMethodID(env->FindClass("java/lang/Class"), "getName", "()Ljava/lang/String;");
        }
    } v { env };
    art::Runtime::Current()->getClassLinker()->VisitClasses(&v);
}

class MyCallback : public art::ClassLoadCallback {
public:
    JavaVM* vm;
    jmethodID getClassName;

    MyCallback(JNIEnv *env) {
        env->GetJavaVM(&vm);
        getClassName = env->GetMethodID(env->FindClass("java/lang/Class"), "getName", "()Ljava/lang/String;");
    }

    void ClassLoad(art::Handle<art::mirror::Class> klass) override {
    }

    void ClassPrepare(art::Handle<art::mirror::Class> temp_klass, art::Handle<art::mirror::Class> klass) override {
        JNIEnv *env = nullptr;
        vm->GetEnv((void**) &env, JNI_VERSION_1_4);
        auto clz = JNIEnvExt::From(env)->NewLocalRef(klass.Get());
        auto str = (jstring) env->CallObjectMethod(clz, getClassName);
        auto chars = env->GetStringUTFChars(str, nullptr);
        LOGD("prepared class %s", chars);
        env->ReleaseStringUTFChars(str, chars);
        JNIEnvExt::From(env)->DeleteLocalRef(clz);
    }
};

extern "C"
JNIEXPORT void JNICALL
Java_io_github_a13e300_tools_NativeUtils_monitorClasses(JNIEnv *env, jclass clazz,
                                                        jboolean enabled) {
    static std::unique_ptr<art::ClassLoadCallback> callback;

    if (!callback && enabled == JNI_TRUE) {
        auto ptr = new MyCallback(env);
        callback.reset(ptr);
        art::Runtime::Current()->GetRuntimeCallbacks()->AddClassLoadCallback(ptr);
    } else if (callback && enabled == JNI_FALSE) {
        auto ptr = callback.release();
        art::Runtime::Current()->GetRuntimeCallbacks()->RemoveClassLoadCallback(ptr);
    }
}
