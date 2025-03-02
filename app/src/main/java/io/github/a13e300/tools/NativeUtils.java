package io.github.a13e300.tools;

import android.os.Build;
import android.os.Debug;
import android.util.Log;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

public class NativeUtils {
    static {
        try {
            System.loadLibrary("stethox");
        } catch (Throwable t) {
            Log.e("StethoX", "failed to load native library", t);
        }
    }

    private static boolean jvmtiAttached = false;

    public static native ClassLoader[] getClassLoaders();

    public static native ClassLoader[] getClassLoaders2();

    private static native void nativeAllowDebugging();
    private static native int nativeSetJavaDebug(boolean allow);
    private static native void nativeRestoreJavaDebug(int orig);

    private static void ensureJvmTi() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            throw new UnsupportedOperationException("JVMTI is unsupported!");
        }
        synchronized (NativeUtils.class) {
            if (jvmtiAttached) return;
            nativeAllowDebugging();
            var orig = nativeSetJavaDebug(true);
            try {
                Debug.attachJvmtiAgent("libstethox.so", "", NativeUtils.class.getClassLoader());
            } catch (Throwable t) {
                Logger.e("load jvmti", t);
                throw new UnsupportedOperationException("load failed", t);
            } finally {
                nativeRestoreJavaDebug(orig);
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

    private static native void dumpThread();

    public static class FrameVar {
        String name;
        String sig;
        int slot;

        Object lvalue;
        long jvalue;
        int ivalue;
        short svalue;
        char cvalue;
        byte bvalue;
        boolean zvalue;
        float fvalue;
        double dvalue;

        public FrameVar() {}
    }

    private static native FrameVar[] getFrameVarsNative(int nframe);

    public static FrameVar[] getFrameVars(int nframe) {
        ensureJvmTi();
        return getFrameVarsNative(nframe);
    }

    public static native Object[] getGlobalRefs(Class<?> clazz);

    public static native Member getCLInit(Class<?> clazz);

    private static native Object invokeNonVirtualInternal(Method method, Class<?> target, byte[] types, Object thiz, Object[] args) throws InvocationTargetException;

    private static byte typeId(Class<?> type) {
        if (type == int.class) {
            return 0;
        } else if (type == long.class) {
            return 1;
        } else if (type == short.class) {
            return 2;
        } else if (type == byte.class) {
            return 3;
        } else if (type == char.class) {
            return 4;
        } else if (type == boolean.class) {
            return 5;
        } else if (type == float.class) {
            return 6;
        } else if (type == double.class) {
            return 7;
        } else if (type == Void.class) {
            return 9;
        }
        return 8;
    }

    public static Object invokeNonVirtual(Method method, Object thiz, Object ...args) throws InvocationTargetException {
        if (thiz == null) throw new NullPointerException("this == null");
        var modifier = method.getModifiers();
        if (Modifier.isStatic(modifier)) throw new IllegalArgumentException("expected instance method, got " + method);
        if (Modifier.isAbstract(modifier)) throw new IllegalArgumentException("cannot invoke abstract method " + method);
        var clazz = method.getDeclaringClass();
        if (!clazz.isInstance(thiz)) throw new IllegalArgumentException(thiz + " is not an instance of class " + clazz);

        Class<?> retType = method.getReturnType();
        Class<?>[] pTypes = method.getParameterTypes();
        byte[] types;
        types = new byte[pTypes.length + 1];
        types[0] = typeId(retType);
        for (int i = 0; i < pTypes.length; i++) {
            types[i + 1] = typeId(pTypes[i]);
        }
        return invokeNonVirtualInternal(method, clazz, types, thiz, args);
    }
}
