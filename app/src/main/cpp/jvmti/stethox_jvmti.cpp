#include "jvmti.h"

#include "logging.h"

#include <vector>
#include <mutex>

static jvmtiEnv *gJvmtiEnv = nullptr;

std::string Format(const char* fmt, ...) {
    va_list ap;
    char buf[1024];
    va_start(ap, fmt);
    vsnprintf(buf, 1024, fmt, ap);
    va_end(ap);
    return buf;
}

std::string to_string(jvmtiError e) {
    const char* name;
    switch (e) {
        case JVMTI_ERROR_NONE: name = "JVMTI_ERROR_NONE"; break;
        case JVMTI_ERROR_INVALID_THREAD: name = "JVMTI_ERROR_INVALID_THREAD"; break;
        case JVMTI_ERROR_INVALID_THREAD_GROUP: name = "JVMTI_ERROR_INVALID_THREAD_GROUP"; break;
        case JVMTI_ERROR_INVALID_PRIORITY: name = "JVMTI_ERROR_INVALID_PRIORITY"; break;
        case JVMTI_ERROR_THREAD_NOT_SUSPENDED: name = "JVMTI_ERROR_THREAD_NOT_SUSPENDED"; break;
        case JVMTI_ERROR_THREAD_SUSPENDED: name = "JVMTI_ERROR_THREAD_SUSPENDED"; break;
        case JVMTI_ERROR_THREAD_NOT_ALIVE: name = "JVMTI_ERROR_THREAD_NOT_ALIVE"; break;
        case JVMTI_ERROR_INVALID_OBJECT: name = "JVMTI_ERROR_INVALID_OBJECT"; break;
        case JVMTI_ERROR_INVALID_CLASS: name = "JVMTI_ERROR_INVALID_CLASS"; break;
        case JVMTI_ERROR_CLASS_NOT_PREPARED: name = "JVMTI_ERROR_CLASS_NOT_PREPARED"; break;
        case JVMTI_ERROR_INVALID_METHODID: name = "JVMTI_ERROR_INVALID_METHODID"; break;
        case JVMTI_ERROR_INVALID_LOCATION: name = "JVMTI_ERROR_INVALID_LOCATION"; break;
        case JVMTI_ERROR_INVALID_FIELDID: name = "JVMTI_ERROR_INVALID_FIELDID"; break;
        case JVMTI_ERROR_NO_MORE_FRAMES: name = "JVMTI_ERROR_NO_MORE_FRAMES"; break;
        case JVMTI_ERROR_OPAQUE_FRAME: name = "JVMTI_ERROR_OPAQUE_FRAME"; break;
        case JVMTI_ERROR_TYPE_MISMATCH: name = "JVMTI_ERROR_TYPE_MISMATCH"; break;
        case JVMTI_ERROR_INVALID_SLOT: name = "JVMTI_ERROR_INVALID_SLOT"; break;
        case JVMTI_ERROR_DUPLICATE: name = "JVMTI_ERROR_DUPLICATE"; break;
        case JVMTI_ERROR_NOT_FOUND: name = "JVMTI_ERROR_NOT_FOUND"; break;
        case JVMTI_ERROR_INVALID_MONITOR: name = "JVMTI_ERROR_INVALID_MONITOR"; break;
        case JVMTI_ERROR_NOT_MONITOR_OWNER: name = "JVMTI_ERROR_NOT_MONITOR_OWNER"; break;
        case JVMTI_ERROR_INTERRUPT: name = "JVMTI_ERROR_INTERRUPT"; break;
        case JVMTI_ERROR_INVALID_CLASS_FORMAT: name = "JVMTI_ERROR_INVALID_CLASS_FORMAT"; break;
        case JVMTI_ERROR_CIRCULAR_CLASS_DEFINITION: name = "JVMTI_ERROR_CIRCULAR_CLASS_DEFINITION"; break;
        case JVMTI_ERROR_FAILS_VERIFICATION: name = "JVMTI_ERROR_FAILS_VERIFICATION"; break;
        case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_ADDED: name = "JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_ADDED"; break;
        case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_SCHEMA_CHANGED: name = "JVMTI_ERROR_UNSUPPORTED_REDEFINITION_SCHEMA_CHANGED"; break;
        case JVMTI_ERROR_INVALID_TYPESTATE: name = "JVMTI_ERROR_INVALID_TYPESTATE"; break;
        case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_HIERARCHY_CHANGED: name = "JVMTI_ERROR_UNSUPPORTED_REDEFINITION_HIERARCHY_CHANGED"; break;
        case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_DELETED: name = "JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_DELETED"; break;
        case JVMTI_ERROR_UNSUPPORTED_VERSION: name = "JVMTI_ERROR_UNSUPPORTED_VERSION"; break;
        case JVMTI_ERROR_NAMES_DONT_MATCH: name = "JVMTI_ERROR_NAMES_DONT_MATCH"; break;
        case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_CLASS_MODIFIERS_CHANGED: name = "JVMTI_ERROR_UNSUPPORTED_REDEFINITION_CLASS_MODIFIERS_CHANGED"; break;
        case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_MODIFIERS_CHANGED: name = "JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_MODIFIERS_CHANGED"; break;
        case JVMTI_ERROR_UNMODIFIABLE_CLASS: name = "JVMTI_ERROR_UNMODIFIABLE_CLASS"; break;
        case JVMTI_ERROR_NOT_AVAILABLE: name = "JVMTI_ERROR_NOT_AVAILABLE"; break;
        case JVMTI_ERROR_MUST_POSSESS_CAPABILITY: name = "JVMTI_ERROR_MUST_POSSESS_CAPABILITY"; break;
        case JVMTI_ERROR_NULL_POINTER: name = "JVMTI_ERROR_NULL_POINTER"; break;
        case JVMTI_ERROR_ABSENT_INFORMATION: name = "JVMTI_ERROR_ABSENT_INFORMATION"; break;
        case JVMTI_ERROR_INVALID_EVENT_TYPE: name = "JVMTI_ERROR_INVALID_EVENT_TYPE"; break;
        case JVMTI_ERROR_ILLEGAL_ARGUMENT: name = "JVMTI_ERROR_ILLEGAL_ARGUMENT"; break;
        case JVMTI_ERROR_NATIVE_METHOD: name = "JVMTI_ERROR_NATIVE_METHOD"; break;
        case JVMTI_ERROR_CLASS_LOADER_UNSUPPORTED: name = "JVMTI_ERROR_CLASS_LOADER_UNSUPPORTED"; break;
        case JVMTI_ERROR_OUT_OF_MEMORY: name = "JVMTI_ERROR_OUT_OF_MEMORY"; break;
        case JVMTI_ERROR_ACCESS_DENIED: name = "JVMTI_ERROR_ACCESS_DENIED"; break;
        case JVMTI_ERROR_WRONG_PHASE: name = "JVMTI_ERROR_WRONG_PHASE"; break;
        case JVMTI_ERROR_INTERNAL: name = "JVMTI_ERROR_INTERNAL"; break;
        case JVMTI_ERROR_UNATTACHED_THREAD: name = "JVMTI_ERROR_UNATTACHED_THREAD"; break;
        case JVMTI_ERROR_INVALID_ENVIRONMENT: name = "JVMTI_ERROR_INVALID_ENVIRONMENT"; break;
        default: name = "UNKNOWN";
    }
    return Format("JVMTI Error %d (%s)", e, name);
}


extern "C"
JNIEXPORT jint JNICALL
Agent_OnAttach(JavaVM* vm, char *options, void *reserved) {
    LOGD("jvmti attached");
    // https://cs.android.com/android/platform/superproject/main/+/main:art/openjdkjvmti/art_jvmti.h;l=72;drc=be282e173efd05b53632fe16d843474368283191
    constexpr jint kArtTiVersion = JVMTI_VERSION_1_2 | 0x40000000;
    vm->GetEnv(reinterpret_cast<void **>(&gJvmtiEnv), kArtTiVersion);
    jvmtiCapabilities cap{};
    cap.can_tag_objects = true;
    auto r = gJvmtiEnv->AddCapabilities(&cap);
    if (r) {
        LOGD("addCapabilities: %s", to_string(r).c_str());
        return 1;
    }
    return 0;
}

static jvmtiError getAssignableClasses(JNIEnv* env, jclass targetClazz, jobject class_loader, std::vector<jclass> &target_classes) {
    jvmtiError r = JVMTI_ERROR_NONE;
    jint count;
    jclass* classes;
    if (class_loader) {
        r = gJvmtiEnv->GetClassLoaderClasses(class_loader, &count, &classes);
        if (r) {
            LOGE("GetClassLoaderClasses %s", to_string(r).c_str());
            return r;
        }
    } else {
        r = gJvmtiEnv->GetLoadedClasses(&count, &classes);
        if (r) {
            LOGE("GetLoadedClasses %s", to_string(r).c_str());
            return r;
        }
    }
    LOGD("class count %d", count);
    for (int i = 0; i < count; i++) {
        if (env->IsAssignableFrom(classes[i], targetClazz)) {
            target_classes.emplace_back(classes[i]);
        }
    }
    gJvmtiEnv->Deallocate(reinterpret_cast<unsigned char*>(classes));
    return r;
}

static std::mutex g_mutex;

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_io_github_a13e300_tools_NativeUtils_nativeGetObjects(JNIEnv *env, jclass, jclass targetClazz, jboolean child) {
    if (!gJvmtiEnv) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "no jvmti env");
        return nullptr;
    }

    std::lock_guard lk{g_mutex};

    jvmtiError r;
    std::vector<jclass> target_classes;
    if (child) {
        r = getAssignableClasses(env, targetClazz, nullptr, target_classes);
        if (r) {
            env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ("getAssignableClasses: " + to_string(r)).c_str());
            return nullptr;
        }
    } else {
        target_classes.emplace_back(targetClazz);
    }

    LOGD("target classes %zu", target_classes.size());
    for (auto clarr: target_classes) {
        r = gJvmtiEnv->SetTag(clarr, 0xfffe);
        if (r) {
            LOGE("SetTag %s", to_string(r).c_str());
            env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ("SetTag: " + to_string(r)).c_str());
            return nullptr;
        }
    }

    jvmtiHeapCallbacks callbacks {};
    callbacks.heap_iteration_callback = [](jlong class_tag, jlong size, jlong* tag_ptr, jint length, void* user_data) -> jint {
        if (class_tag == 0xfffe) *tag_ptr = 0xffff;
        return JVMTI_VISIT_OBJECTS;
    };

    r = gJvmtiEnv->IterateThroughHeap(0, nullptr, &callbacks, nullptr);
    if (r) {
        LOGE("IterateThroughHeap: %d", r);
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ("IterateThroughHeap: " + to_string(r)).c_str());
        return nullptr;
    }

    jlong the_tag = 0xffff;
    jint count;
    jobject* objects;
    r = gJvmtiEnv->GetObjectsWithTags(1, &the_tag, &count, &objects, nullptr);
    if (r) {
        LOGE("GetObjectsWithTags: %d", r);
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ("GetObjectsWithTags: " + to_string(r)).c_str());
        return nullptr;
    }

    LOGD("got objects %d", count);
    auto arr = env->NewObjectArray(count, env->FindClass("java/lang/Object"), nullptr);
    for (int i = 0; i < count; i++) {
        env->SetObjectArrayElement(arr, i, objects[i]);
        gJvmtiEnv->SetTag(objects[i], 0);
    }
    gJvmtiEnv->Deallocate(reinterpret_cast<unsigned char*>(objects));

    for (auto clarr: target_classes) {
        r = gJvmtiEnv->SetTag(clarr, 0);
        if (r) {
            LOGE("SetTag %s", to_string(r).c_str());
            env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ("SetTag: " + to_string(r)).c_str());
            return nullptr;
        }
    }

    return arr;
}

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_io_github_a13e300_tools_NativeUtils_nativeGetAssignableClasses(JNIEnv *env, jclass,
                                                              jclass targetClazz, jobject loader) {
    if (!gJvmtiEnv) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "no jvmti env");
        return nullptr;
    }

    std::lock_guard lk{g_mutex};

    std::vector<jclass> target_classes;
    auto r = getAssignableClasses(env, targetClazz, loader, target_classes);
    if (r) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ("getAssignableClasses: " + to_string(r)).c_str());
        return nullptr;
    }
    auto sz = static_cast<int>(target_classes.size());
    auto arr = env->NewObjectArray(sz, env->FindClass("java/lang/Class"), nullptr);
    for (int i = 0; i < sz; i++) {
        env->SetObjectArrayElement(arr, i, target_classes[i]);
    }
    return arr;
}
