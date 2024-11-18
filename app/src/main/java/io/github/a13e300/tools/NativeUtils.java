package io.github.a13e300.tools;

import android.os.Build;
import android.os.Debug;
import android.util.Log;

public class NativeUtils {
    static {
        try {
            System.loadLibrary("stethox");
            Log.d("StethoX", "native init = " + initNative());
        } catch (Throwable t) {
            Log.e("StethoX", "failed to load native library", t);
        }
    }

    private static boolean jvmtiAttached = false;

    public static native ClassLoader[] getClassLoaders();
    public static native boolean initNative();

    public static native ClassLoader[] getClassLoaders2();

    private static native void nativeAllowDebugging();
    private static native void nativeSetJavaDebug(boolean allow);

    private static void ensureJvmTi() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            throw new UnsupportedOperationException("JVMTI is unsupported!");
        }
        synchronized (NativeUtils.class) {
            if (jvmtiAttached) return;
            nativeAllowDebugging();
            nativeSetJavaDebug(true);
            try {
                Debug.attachJvmtiAgent("libstethox.so", "", NativeUtils.class.getClassLoader());
            } catch (Throwable t) {
                Logger.e("load jvmti", t);
                throw new UnsupportedOperationException("load failed", t);
            } finally {
                nativeSetJavaDebug(false);
            }
            jvmtiAttached = true;
        }
    }

    public static Object[] getObjects(Class<?> clazz, boolean child) {
        ensureJvmTi();
        return nativeGetObjects(clazz, child);
    }

    public static Class<?>[] getAssignableClasses(Class<?> clazz, ClassLoader loader) {
        ensureJvmTi();
        return nativeGetAssignableClasses(clazz, loader);
    }

    private static native Object[] nativeGetObjects(Class<?> clazz, boolean child);

    private static native Class<?>[] nativeGetAssignableClasses(Class<?> clazz, ClassLoader loader);
}
