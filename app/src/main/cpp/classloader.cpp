#include <jni.h>
#include <fcntl.h>
#include <unistd.h>
#include "plt.h"
#include "classloader.h"
#include "logging.h"
#include "art.h"
#include <functional>
#include <utility>

static jobject newLocalRef(JNIEnv *env, void *object) {
    static jobject (*NewLocalRef)(JNIEnv *, void *) = nullptr;
    if (object == nullptr) {
        return nullptr;
    }
    if (NewLocalRef == nullptr) {
        NewLocalRef = (jobject (*)(JNIEnv *, void *)) plt_dlsym("_ZN3art9JNIEnvExt11NewLocalRefEPNS_6mirror6ObjectE", nullptr);
        LOGD("NewLocalRef: %p", NewLocalRef);
    }
    if (NewLocalRef != nullptr) {
        return NewLocalRef(env, object);
    } else {
        return nullptr;
    }
}

static void deleteLocalRef(JNIEnv *env, jobject object) {
    static void (*DeleteLocalRef)(JNIEnv *, jobject) = nullptr;
    if (DeleteLocalRef == nullptr) {
        DeleteLocalRef = (void (*)(JNIEnv *, jobject)) plt_dlsym("_ZN3art9JNIEnvExt14DeleteLocalRefEP8_jobject", nullptr);
        LOGD("DeleteLocalRef: %p", DeleteLocalRef);
    }
    if (DeleteLocalRef != nullptr) {
        DeleteLocalRef(env, object);
    }
}

using Callback = std::function<void(jobject)>;

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
        jobject object = newLocalRef((JNIEnv *) env_, (jobject) root);
        if (object != nullptr) {
            auto s = (jstring) env_->CallObjectMethod(env_->GetObjectClass(object), toStringMid);
            auto c = env_->GetStringUTFChars(s, nullptr);
            LOGD("class name %s", c);
            env_->ReleaseStringUTFChars(s, c);
            if (env_->IsInstanceOf(object, classLoader_)) {
                LOGD("object %p", object);
                callback_(object);
            }
            deleteLocalRef((JNIEnv *) env_, object);
        }
    }

private:
    JNIEnv *env_;
    jclass classLoader_;
};

static void checkGlobalRef(JNIEnv *env, jclass clazz, Callback cb) {
    auto VisitRoots = (void (*)(void *, void *)) plt_dlsym("_ZN3art9JavaVMExt10VisitRootsEPNS_11RootVisitorE", nullptr);
#ifdef DEBUG
    LOGI("VisitRoots: %p", VisitRoots);
#endif
    if (VisitRoots == nullptr) {
        return;
    }
    JavaVM *jvm;
    env->GetJavaVM(&jvm);
    ClassLoaderVisitor visitor(env, clazz, std::move(cb));
    VisitRoots(jvm, &visitor);
}

class MyClassLoaderVisitor : public art::ClassLoaderVisitor {
public:
    MyClassLoaderVisitor(JNIEnv *env, Callback callback) : env_(env), callback_(std::move(callback)) {
    }

    void Visit(art::mirror::ClassLoader *class_loader) override {
        jobject object = newLocalRef((JNIEnv *) env_, (jobject) class_loader);
        if (object != nullptr) {
            LOGD("object %p", object);
            callback_(object);
            deleteLocalRef((JNIEnv *) env_, object);
        }
    }

private:
    JNIEnv *env_;
    Callback callback_;
};

void visitClassLoaders(JNIEnv *env, jobject cb) {
    jclass callbackClass = env->FindClass("io/github/a13e300/tools/ObjectEnumerator");
    if (env->ExceptionCheck()) {
#ifdef DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
    }
    if (callbackClass == nullptr) {
        LOGE("callback class is null");
        return;
    }
    auto method = env->GetMethodID(callbackClass, "onEnumerate", "(Ljava/lang/Object;)V");

    auto callback = [&](jobject o) {
        env->CallVoidMethod(cb, method, o);
    };
    MyClassLoaderVisitor v(env, callback);
    art::Runtime::Current()->getClassLinker()->VisitClassLoaders(&v);
}

/*
class WeakClassLoaderVisitor : public art::IsMarkedVisitor {
public :
    WeakClassLoaderVisitor(C_JNIEnv *env, jclass classLoader) : env_(env), classLoader_(classLoader) {
    }

    art::mirror::Object *IsMarked(art::mirror::Object *obj) override {
        jobject object = newLocalRef((JNIEnv *) env_, (jobject) obj);
        if (object != nullptr) {
            if ((*env_)->IsInstanceOf((JNIEnv *) env_, object, classLoader_)) {
                // ...
            }
            deleteLocalRef((JNIEnv *) env_, object);
        }
        return obj;
    }

private:
    C_JNIEnv *env_;
    jclass classLoader_;
};

static void checkWeakGlobalRef(C_JNIEnv *env, jclass clazz) {
    auto SweepJniWeakGlobals = (void (*)(void *, void *)) plt_dlsym("_ZN3art9JavaVMExt19SweepJniWeakGlobalsEPNS_15IsMarkedVisitorE", nullptr);
#ifdef DEBUG
    LOGI("SweepJniWeakGlobals: %p", SweepJniWeakGlobals);
#endif
    if (SweepJniWeakGlobals == nullptr) {
        return;
    }
    JavaVM *jvm;
    (*env)->GetJavaVM((JNIEnv *) env, &jvm);
    WeakClassLoaderVisitor visitor(env, clazz);
    SweepJniWeakGlobals(jvm, &visitor);
}
*/

void enumerateClassLoader(JNIEnv *env, jobject cb) {
    jclass clazz = env->FindClass("java/lang/ClassLoader");
    if (env->ExceptionCheck()) {
#ifdef DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
    }
    if (clazz == nullptr) {
        LOGE("classloader is null");
        return;
    }

    jclass callbackClass = env->FindClass("io/github/a13e300/tools/ObjectEnumerator");
    if (env->ExceptionCheck()) {
#ifdef DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
    }
    if (callbackClass == nullptr) {
        LOGE("callback class is null");
        return;
    }
    auto method = env->GetMethodID(callbackClass, "onEnumerate", "(Ljava/lang/Object;)V");

    auto callback = [&](jobject o) {
        env->CallVoidMethod(cb, method, o);
    };

    checkGlobalRef(env, clazz, callback);
    // checkWeakGlobalRef(env, clazz);
    env->DeleteLocalRef(clazz);
}
