#include <jni.h>

// Write C++ code here.
//
// Do not forget to dynamically load the C++ library into your application.
//
// For instance,
//
// In MainActivity.java:
//    static {
//       System.loadLibrary("stethox");
//    }
//
// Or, in MainActivity.kt:
//    companion object {
//      init {
//         System.loadLibrary("stethox")
//      }
//    }
#include "classloader.h"
#include "art.h"

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