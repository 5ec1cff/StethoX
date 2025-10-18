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
    // 这个 cap 是给 getFrameVarsNative 用的，但是
    // openjdkjvmti::EventHandler::HandleLocalAccessCapabilityAdded 会调用 ReinitializeMethodsCode / InitializeMethodsCode
    // 因此方法入口会被替换成解释器导致 hook 失效
    // 考虑到 getFrameVarsNative 效果不是很好，因此这个 cap 也暂时没必要使用
    // cap.can_access_local_variables = true;
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

extern "C"
JNIEXPORT void JNICALL
Java_io_github_a13e300_tools_NativeUtils_dumpThread(JNIEnv *env, jclass clazz) {
    jvmtiError r;
    jint frame_count;
    jvmtiFrameInfo frames[128];
    jmethodID to_string_method;
    {
        auto clz = env->FindClass("java/lang/Object");
        to_string_method = env->GetMethodID(clz, "toString", "()Ljava/lang/String;");
        env->DeleteLocalRef(clz);
    }
    r = gJvmtiEnv->GetStackTrace(nullptr, 0, 128, frames, &frame_count);
    if (r) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ("GetStackTrace: " + to_string(r)).c_str());
        return;
    }
    LOGD("frame count: %d", frame_count);
    for (jint i = 0; i < frame_count; i++) {
        LOGD("frame %d", i);
        auto &frame = frames[i];
        jmethodID method = frame.method;
        char *method_name, *method_signature, *class_signature;
        jclass declaring_class;
        jint max_locals, local_variable_count;
        jvmtiLocalVariableEntry *local_variables;
        r = gJvmtiEnv->GetMethodName(method, &method_name, &method_signature, nullptr);
        if (r) {
            LOGD("GetMethodName: %s", to_string(r).c_str());
            return;
        }
        r = gJvmtiEnv->GetMethodDeclaringClass(method, &declaring_class);
        if (r) {
            LOGD("GetMethodDeclaringClass: %s", to_string(r).c_str());
            goto free_method;
        }
        r = gJvmtiEnv->GetClassSignature(declaring_class, &class_signature, nullptr);
        if (r) {
            LOGD("GetClassSignature: %s", to_string(r).c_str());
            goto free_class_ref;
        }
        LOGD("method %s#%s(%s)@%lld", class_signature, method_name, method_signature, frame.location);
        r = gJvmtiEnv->GetMaxLocals(method, &max_locals);
        if (r) {
            LOGD("GetMaxLocals: %s", to_string(r).c_str());
            goto free_class;
        }
        LOGD("max local: %d", max_locals);
        r = gJvmtiEnv->GetLocalVariableTable(method, &local_variable_count, &local_variables);
        if (r) {
            LOGD("GetLocalVariableTable: %s", to_string(r).c_str());
            goto free_class;
        }
        LOGD("local variable count: %d", local_variable_count);
        for (jint j = 0; j < local_variable_count; j++) {
            auto &v = local_variables[j];
            if (frame.location >= v.start_location && frame.location < v.start_location + v.length) {
                LOGD("var %d start %lld length %d name %s signature %s generic %s slot %d",
                     j, v.start_location, v.length, v.name, v.signature, v.generic_signature, v.slot
                );
                jobject value;
                r = gJvmtiEnv->GetLocalObject(nullptr, i, v.slot, &value);
                if (r) {
                    LOGD("GetLocalObject: %s", to_string(r).c_str());
                } else {
                    if (!value) {
                        LOGD("value: null");
                    } else {
                        LOGD("value:");
                        auto klazz = env->GetObjectClass(value);
                        char* clazz_signature;
                        r = gJvmtiEnv->GetClassSignature(klazz, &clazz_signature, nullptr);
                        if (r) {
                            LOGD("GetClassSignature: %s", to_string(r).c_str());
                        } else {
                            LOGD("type: %s", clazz_signature);
                            gJvmtiEnv->Deallocate((unsigned char*) clazz_signature);
                        }
                        jboolean is_array = JNI_FALSE;
                        r = gJvmtiEnv->IsArrayClass(klazz, &is_array);
                        if (r) {
                            LOGE("IsArrayClass: %s", to_string(r).c_str());
                        }
                        if (is_array) {

                        } else {
                            auto str = (jstring) env->CallObjectMethod(value, to_string_method);
                            auto chars = env->GetStringUTFChars(str, nullptr);
                            LOGD("string: %s", chars);
                            env->ReleaseStringUTFChars(str, chars);
                        }
                        env->DeleteLocalRef(value);
                    }
                }
            }
        }

        free_local_variables:
        gJvmtiEnv->Deallocate((unsigned char*) local_variables);
        free_class:
        gJvmtiEnv->Deallocate((unsigned char*) class_signature);
        free_class_ref:
        env->DeleteLocalRef(declaring_class);
        free_method:
        gJvmtiEnv->Deallocate((unsigned char*) method_signature);
        gJvmtiEnv->Deallocate((unsigned char*) method_name);

    }
}

#define THROW(s) env->ThrowNew(env->FindClass("java/lang/RuntimeException"), (s))
#define THROWS(s) env->ThrowNew(env->FindClass("java/lang/RuntimeException"), (s).c_str())
#define THROWR(s) THROWS(s ": " + std::to_string(r))

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_io_github_a13e300_tools_NativeUtils_getFrameVarsNative(JNIEnv *env, jclass clazz, jint nframe) {
    jvmtiError r;
    jclass class_frame_var = env->FindClass("io/github/a13e300/tools/NativeUtils$FrameVar");
    jfieldID field_name = env->GetFieldID(class_frame_var, "name", "Ljava/lang/String;");
    jfieldID field_sig = env->GetFieldID(class_frame_var, "sig", "Ljava/lang/String;");
    jfieldID field_slot = env->GetFieldID(class_frame_var, "slot", "I");
    jfieldID field_lvalue = env->GetFieldID(class_frame_var, "lvalue", "Ljava/lang/Object;");
    jfieldID field_zvalue = env->GetFieldID(class_frame_var, "zvalue", "Z");
    jfieldID field_bvalue = env->GetFieldID(class_frame_var, "bvalue", "B");
    jfieldID field_cvalue = env->GetFieldID(class_frame_var, "cvalue", "C");
    jfieldID field_svalue = env->GetFieldID(class_frame_var, "svalue", "S");
    jfieldID field_ivalue = env->GetFieldID(class_frame_var, "ivalue", "I");
    jfieldID field_jvalue = env->GetFieldID(class_frame_var, "jvalue", "J");
    jfieldID field_fvalue = env->GetFieldID(class_frame_var, "fvalue", "F");
    jfieldID field_dvalue = env->GetFieldID(class_frame_var, "dvalue", "D");
    jmethodID cstr = env->GetMethodID(class_frame_var, "<init>", "()V");
    std::vector<jobject> results;

    jint frame_count;
    r = gJvmtiEnv->GetFrameCount(nullptr, &frame_count);
    if (r) {
        THROWR("GetFrameCount");
        return nullptr;
    }
    if (nframe < 0) {
        nframe += frame_count;
    }

    if (nframe < 0 || nframe >= frame_count) {
        THROW("invalid frame");
        return nullptr;
    }

    LOGD("get frame at %d", nframe);

    jvmtiFrameInfo frame;

    r = gJvmtiEnv->GetStackTrace(nullptr, nframe, 1, &frame, &frame_count);
    if (r) {
        THROWR("GetStackTrace");
        return nullptr;
    }
    if (frame_count != 1) {
        THROW("wrong frame count");
        return nullptr;
    }

    {
        jobject o = nullptr;
        r = gJvmtiEnv->GetLocalInstance(nullptr, nframe, &o);
        if (r) {
            LOGE("GetLocalInstance %s", to_string(r).c_str());
        } else if (o) {
            auto obj = env->NewObject(class_frame_var, cstr);
            env->SetObjectField(obj, field_name, env->NewStringUTF("this"));
            env->SetObjectField(obj, field_sig, env->NewStringUTF("this"));
            env->SetIntField(obj, field_slot, 0);
            env->SetObjectField(obj, field_lvalue, o);
            results.push_back(obj);
            env->DeleteLocalRef(o);
        }
    }

    jint local_variable_count;
    jvmtiLocalVariableEntry *local_variables;
    r = gJvmtiEnv->GetLocalVariableTable(frame.method, &local_variable_count, &local_variables);
    if (r) {
        LOGE("GetLocalVariableTable %s", to_string(r).c_str());
    } else {
        LOGD("local variable count: %d", local_variable_count);
        for (jint j = 0; j < local_variable_count; j++) {
            auto &v = local_variables[j];
            if (frame.location >= v.start_location &&
                frame.location < v.start_location + v.length) {
                LOGD("var %d start %lld length %d name %s signature %s generic %s slot %d",
                     j, v.start_location, v.length, v.name, v.signature, v.generic_signature, v.slot
                );
                auto obj = env->NewObject(class_frame_var, cstr);
                env->SetObjectField(obj, field_name, env->NewStringUTF(v.name));
                env->SetObjectField(obj, field_sig, env->NewStringUTF(v.signature));
                env->SetIntField(obj, field_slot, v.slot);

                if (v.signature && v.signature[0]) {
                    if (v.signature[0] == '[' || v.signature[0] == 'L') {
                        jobject val;
                        r = gJvmtiEnv->GetLocalObject(nullptr, nframe, v.slot, &val);
                        if (r) {
                            LOGD("GetLocalObject: %s", to_string(r).c_str());
                        } else {
                            env->SetObjectField(obj, field_lvalue, val);
                        }
                    } else if (v.signature[0] == 'Z') {
                        jint val;
                        r = gJvmtiEnv->GetLocalInt(nullptr, nframe, v.slot, &val);
                        if (r) {
                            LOGD("GetLocalInt: %s", to_string(r).c_str());
                        } else {
                            env->SetBooleanField(obj, field_zvalue, val);
                        }
                    } else if (v.signature[0] == 'B') {
                        jint val;
                        r = gJvmtiEnv->GetLocalInt(nullptr, nframe, v.slot, &val);
                        if (r) {
                            LOGD("GetLocalInt: %s", to_string(r).c_str());
                        } else {
                            env->SetByteField(obj, field_bvalue, val);
                        }
                    } else if (v.signature[0] == 'C') {
                        jint val;
                        r = gJvmtiEnv->GetLocalInt(nullptr, nframe, v.slot, &val);
                        if (r) {
                            LOGD("GetLocalInt: %s", to_string(r).c_str());
                        } else {
                            env->SetCharField(obj, field_cvalue, val);
                        }
                    } else if (v.signature[0] == 'S') {
                        jint val;
                        r = gJvmtiEnv->GetLocalInt(nullptr, nframe, v.slot, &val);
                        if (r) {
                            LOGD("GetLocalInt: %s", to_string(r).c_str());
                        } else {
                            env->SetShortField(obj, field_svalue, val);
                        }
                    } else if (v.signature[0] == 'I') {
                        jint val;
                        r = gJvmtiEnv->GetLocalInt(nullptr, nframe, v.slot, &val);
                        if (r) {
                            LOGD("GetLocalInt: %s", to_string(r).c_str());
                        } else {
                            env->SetIntField(obj, field_ivalue, val);
                        }
                    } else if (v.signature[0] == 'J') {
                        jlong val;
                        r = gJvmtiEnv->GetLocalLong(nullptr, nframe, v.slot, &val);
                        if (r) {
                            LOGD("GetLocalObject: %s", to_string(r).c_str());
                        } else {
                            env->SetLongField(obj, field_jvalue, val);
                        }
                    } else if (v.signature[0] == 'F') {
                        jfloat val;
                        r = gJvmtiEnv->GetLocalFloat(nullptr, nframe, v.slot, &val);
                        if (r) {
                            LOGD("GetLocalObject: %s", to_string(r).c_str());
                        } else {
                            env->SetFloatField(obj, field_fvalue, val);
                        }
                    } else if (v.signature[0] == 'D') {
                        jdouble val;
                        r = gJvmtiEnv->GetLocalDouble(nullptr, nframe, v.slot, &val);
                        if (r) {
                            LOGD("GetLocalObject: %s", to_string(r).c_str());
                        } else {
                            env->SetDoubleField(obj, field_dvalue, val);
                        }
                    }
                }
                if (r) {
                    env->DeleteLocalRef(obj);
                } else {
                    results.push_back(obj);
                }
            }
        }

        gJvmtiEnv->Deallocate((unsigned char *) local_variables);
    }

    auto arr = env->NewObjectArray((int) results.size(), class_frame_var, nullptr);
    for (int i = 0; i < (int) results.size(); i++) {
        env->SetObjectArrayElement(arr, i, results[i]);
    }

    return arr;
}
