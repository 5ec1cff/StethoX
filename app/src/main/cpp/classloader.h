#pragma once

#include <jni.h>

jobjectArray visitClassLoaders(JNIEnv *env);
jobjectArray visitClassLoadersByRootVisitor(JNIEnv *env);
