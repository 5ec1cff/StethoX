#include <jni.h>

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
extern "C"
JNIEXPORT jobjectArray JNICALL
Java_io_github_a13e300_tools_NativeUtils_getClassLoaders2(JNIEnv *env, jclass clazz) {
    return visitClassLoadersByRootVisitor(env);
}
