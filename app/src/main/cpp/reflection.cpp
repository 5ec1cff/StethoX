#include "reflection.hpp"

#include "logging.h"


namespace Reflection {
    // I, J, S, B, C, Z, F, D
    // L, V
    jmethodID valueOfMethods[8];
    jmethodID valueMethods[8];
    jclass primitiveClasses[8];
    jmethodID invocationTargetExceptionConstructor;
    jclass invocationTargetExceptionClass;

    bool Init(JNIEnv *env) {
#define CHECK_JNI(cond, ...) \
        if ((cond)) {                                                       \
            LOGE(__VA_ARGS__);                                              \
            env->ExceptionDescribe();                                       \
            env->ExceptionClear();                                          \
            return JNI_ERR;                                                 \
        }

#define FIND_CLASS(className, name) \
        jclass class_##name = env->FindClass(className); \
        CHECK_JNI(!class_##name || env->ExceptionCheck(), "class java.lang.%s not found", #className)

#define FIND_PRIMITIVE_CLASS(idx, cn) \
        FIND_CLASS("java/lang/" #cn, cn)                    \
        primitiveClasses[idx] = reinterpret_cast<jclass>(env->NewGlobalRef(class_##cn));

        FIND_PRIMITIVE_CLASS(0, Integer)
        FIND_PRIMITIVE_CLASS(1, Long)
        FIND_PRIMITIVE_CLASS(2, Short)
        FIND_PRIMITIVE_CLASS(3, Byte)
        FIND_PRIMITIVE_CLASS(4, Character)
        FIND_PRIMITIVE_CLASS(5, Boolean)
        FIND_PRIMITIVE_CLASS(6, Float)
        FIND_PRIMITIVE_CLASS(7, Double)

        FIND_CLASS("java/lang/reflect/InvocationTargetException", InvocationTargetException)
        invocationTargetExceptionClass = reinterpret_cast<jclass>(env->NewGlobalRef(class_InvocationTargetException));
        invocationTargetExceptionConstructor = env->GetMethodID(class_InvocationTargetException, "<init>", "(Ljava/lang/Throwable;)V");
        CHECK_JNI(!invocationTargetExceptionConstructor || env->ExceptionCheck(), "constructor InvocationTargetException(Throwable) not found")

#undef FIND_PRIMITIVE_CLASS
#undef FIND_CLASS

#define FIND_VALUEOF_METHOD(idx, clazz, type) \
        valueOfMethods[idx] = env->GetStaticMethodID(class_##clazz, "valueOf", "(" #type ")Ljava/lang/" #clazz ";"); \
        CHECK_JNI(!valueOfMethods[idx] || env->ExceptionCheck(), "method %s.valueOf() not found", #clazz)

        FIND_VALUEOF_METHOD(0, Integer, I)
        FIND_VALUEOF_METHOD(1, Long, J)
        FIND_VALUEOF_METHOD(2, Short, S)
        FIND_VALUEOF_METHOD(3, Byte, B)
        FIND_VALUEOF_METHOD(4, Character, C)
        FIND_VALUEOF_METHOD(5, Boolean, Z)
        FIND_VALUEOF_METHOD(6, Float, F)
        FIND_VALUEOF_METHOD(7, Double, D)
#undef FIND_VALUEOF_METHOD

#define FIND_VALUE_METHOD(idx, clazz, type, prefix) \
        valueMethods[idx] = env->GetMethodID(class_##clazz, #prefix "Value", "()" #type); \
        CHECK_JNI(!valueMethods[idx] || env->ExceptionOccurred(), "method %s.%sValue() not found", #clazz, #prefix)

        FIND_VALUE_METHOD(0, Integer, I, int)
        FIND_VALUE_METHOD(1, Long, J, long)
        FIND_VALUE_METHOD(2, Short, S, short)
        FIND_VALUE_METHOD(3, Byte, B, byte)
        FIND_VALUE_METHOD(4, Character, C, char)
        FIND_VALUE_METHOD(5, Boolean, Z, boolean)
        FIND_VALUE_METHOD(6, Float, F, float)
        FIND_VALUE_METHOD(7, Double, D, double)
#undef FIND_VALUE_METHOD

#undef CHECK_JNI
        return JNI_VERSION_1_4;
    }

    jobject invokeNonVirtualMethod(JNIEnv *env, jobject method, jclass clazz, jbyteArray types, jobject thiz, jobjectArray argArr) {
        auto mid = env->FromReflectedMethod(method);
        auto len = env->GetArrayLength(types);
        auto typeIds = env->GetByteArrayElements(types, nullptr);

        jvalue argValues[len - 1];
        for (int i = 1; i < len; i++) {
            auto arg = env->GetObjectArrayElement(argArr, i - 1);
            auto typeId = typeIds[i];
            switch (typeId) {
                case 0:
                    argValues[i].i = env->CallIntMethod(arg, valueMethods[0]);
                    break;
                case 1:
                    argValues[i].j = env->CallLongMethod(arg, valueMethods[1]);
                    break;
                case 2:
                    argValues[i].s = env->CallShortMethod(arg, valueMethods[2]);
                    break;
                case 3:
                    argValues[i].b = env->CallByteMethod(arg, valueMethods[3]);
                    break;
                case 4:
                    argValues[i].c = env->CallCharMethod(arg, valueMethods[4]);
                    break;
                case 5:
                    argValues[i].z = env->CallBooleanMethod(arg, valueMethods[5]);
                    break;
                case 6:
                    argValues[i].f = env->CallFloatMethod(arg, valueMethods[6]);
                    break;
                case 7:
                    argValues[i].d = env->CallDoubleMethod(arg, valueMethods[7]);
                    break;
                default:
                    argValues[i].l = arg;
                    break;
            }
        }
        auto retTypeId = typeIds[0];
        env->ReleaseByteArrayElements(types, typeIds, JNI_ABORT);

        jvalue retVal;

        switch (retTypeId) {
            case 0:
                retVal.i = env->CallNonvirtualIntMethodA(thiz, clazz, mid, argValues);
                break;
            case 1:
                retVal.j = env->CallNonvirtualLongMethodA(thiz, clazz, mid, argValues);
                break;
            case 2:
                retVal.s = env->CallNonvirtualShortMethodA(thiz, clazz, mid, argValues);
                break;
            case 3:
                retVal.b = env->CallNonvirtualByteMethodA(thiz, clazz, mid, argValues);
                break;
            case 4:
                retVal.c = env->CallNonvirtualCharMethodA(thiz, clazz, mid, argValues);
                break;
            case 5:
                retVal.z = env->CallNonvirtualBooleanMethodA(thiz, clazz, mid, argValues);
                break;
            case 6:
                retVal.f = env->CallNonvirtualFloatMethodA(thiz, clazz, mid, argValues);
                break;
            case 7:
                retVal.d = env->CallNonvirtualDoubleMethodA(thiz, clazz, mid, argValues);
                break;
            case 8:
                retVal.l = env->CallNonvirtualObjectMethodA(thiz, clazz, mid, argValues);
                break;
            case 9:
                env->CallNonvirtualIntMethodA(thiz, clazz, mid, argValues);
                break;
        }

        if (auto exception = env->ExceptionOccurred(); exception) {
            env->ExceptionClear();
            auto throwable = reinterpret_cast<jthrowable>(env->NewObject(invocationTargetExceptionClass, invocationTargetExceptionConstructor, exception));
            env->Throw(throwable);
            return nullptr;
        }

        jobject ret;

        if (retTypeId >= 0 && retTypeId < 8) {
            ret = env->CallStaticObjectMethodA(primitiveClasses[retTypeId], valueOfMethods[retTypeId], &retVal);
        } else if (retTypeId == 8) {
            ret = retVal.l;
        } else {
            ret = nullptr;
        }

        return ret;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_io_github_a13e300_tools_NativeUtils_invokeNonVirtualInternal(
        JNIEnv *env, jclass,
        jobject method, jclass clazz,
        jbyteArray types, jobject thiz,
        jobjectArray argArr
        ) {
    return Reflection::invokeNonVirtualMethod(env, method, clazz, types, thiz, argArr);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_io_github_a13e300_tools_NativeUtils_getCLInit(JNIEnv *env, jclass, jclass target) {
    auto method = env->GetStaticMethodID(target, "<clinit>", "()V");
    if (method == nullptr || env->ExceptionOccurred()) {
        env->ExceptionClear();
        return nullptr;
    } else {
        return env->ToReflectedMethod(target, method, true);
    }
}
