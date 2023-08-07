#pragma once

#include <jni.h>

void enumerateClassLoader(JNIEnv *env, jobject cb);
void visitClassLoaders(JNIEnv *env, jobject cb);
