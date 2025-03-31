package io.github.a13e300.tools;

import android.annotation.SuppressLint;
import android.os.Build;
import android.os.Debug;
import android.util.Log;

import java.lang.reflect.Array;
import java.lang.reflect.Executable;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

import dalvik.annotation.optimization.FastNative;
import dalvik.system.BaseDexClassLoader;

public class NativeUtils {
    private static final Field artMethodField;
    static {
        Field field = null;
        try {
            System.loadLibrary("stethox");

            field = Executable.class.getDeclaredField("artMethod");
            field.setAccessible(true);
        } catch (Throwable t) {
            Log.e("StethoX", "failed to load native library", t);
        }
        artMethodField = field;
    }

    private static boolean jvmtiAttached = false;

    public static native ClassLoader[] getClassLoaders();

    public static native ClassLoader[] getClassLoaders2();

    // use FastNative to prevent from using ScopedObjectAccess
    @FastNative
    public static native void visitClasses();

    public static native void monitorClasses(boolean enabled);

    private static native int nativeSetJavaDebug(boolean allow, int orig);
    private static native boolean nativeSetJdwp(boolean allow, boolean orig);

    private static void ensureJvmTi() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            throw new UnsupportedOperationException("JVMTI is unsupported!");
        }
        synchronized (NativeUtils.class) {
            if (jvmtiAttached) return;
            var origJdwp = nativeSetJdwp(true, true);
            var orig = nativeSetJavaDebug(true, 0);
            try {
                Debug.attachJvmtiAgent("libstethox.so", "", NativeUtils.class.getClassLoader());
            } catch (Throwable t) {
                Logger.e("load jvmti", t);
                throw new UnsupportedOperationException("load failed", t);
            } finally {
                nativeSetJavaDebug(false, orig);
                nativeSetJdwp(false, origJdwp);
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

    public static void dummy() {

    }

    private static native String nativeReadOatPath(long addr);

    @SuppressLint("DiscouragedPrivateApi")
    public static String getOatPath(BaseDexClassLoader classLoader) {
        var result = new StringBuilder();
        try {
            var fPathList = BaseDexClassLoader.class.getDeclaredField("pathList");
            var fDexElements = fPathList.getType().getDeclaredField("dexElements");
            var fDexFile = fDexElements.getType().getComponentType().getDeclaredField("dexFile");
            var fCookie = fDexFile.getType().getDeclaredField("mCookie");
            fPathList.setAccessible(true);
            fDexElements.setAccessible(true);
            fDexFile.setAccessible(true);
            fCookie.setAccessible(true);
            var pathList = fPathList.get(classLoader);
            var elements = fDexElements.get(pathList);
            var cnt = Array.getLength(elements);
            for (int i = 0; i < cnt; i++) {
                var o = Array.get(elements, i);
                var dexFile = fDexFile.get(o);
                var cookie = fCookie.get(dexFile);
                if (i != 0) result.append(",");
                result.append(nativeReadOatPath(Array.getLong(cookie, 0)));
            }
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
        return result.toString();
    }

    private static native boolean nativeJitCompile(long artMethod, int compileKind, boolean prejit);

    private static native long nativeGetMethodEntry(long artMethod);
    private static native long nativeGetMethodData(long artMethod);
    private static native String nativeGetAddrInfo(long addr);

    private static long getArtMethod(Executable method) {
        try {
            return (long) artMethodField.get(method);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    public static long getMethodEntry(Executable method) {
        return nativeGetMethodEntry(getArtMethod(method));
    }

    public static String getMethodEntryInfo(Executable method) {
        return nativeGetAddrInfo(getMethodEntry(method));
    }

    public static long getMethodData(Executable method) {
        return nativeGetMethodData(getArtMethod(method));
    }

    public static String getMethodDataInfo(Executable method) {
        return nativeGetAddrInfo(getMethodData(method));
    }

    public static boolean jitCompile(Executable method, String compileKind) {
        try {
            int compileKindInt;
            if ("osr".equals(compileKind)) {
                compileKindInt = 0;
            } else if ("baseline".equals(compileKind)) {
                compileKindInt = 1;
            } else if ("optimized".equals(compileKind)) {
                compileKindInt = 2;
            } else {
                throw new IllegalArgumentException("unknown compile kind, should be one of: osr, baseline, optimized");
            }

            var artMethod = getArtMethod(method);

            return nativeJitCompile(artMethod, compileKindInt, false);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    public static int a0() {
        return 1;
    }

    public static int a1() {
        return a0();
    }

    public static int a2() {
        return a1() + 1;
    }

    public static int a3() {
        return a2() + 2;
    }

    public static int a4() {
        return a3() + 3;
    }

    static int theField;

    static int getField1() {
        return theField + 1;
    }

    static int getField2() {
        return getField1() + 2;
    }

    static void printStack1() {
        Log.d("a", "a", new Throwable());
    }

    static void printStack2() {
        Log.d("b", "b", new Throwable());
        printStack1();
    }

/*
    private static boolean typeEquals(Class<?> oType, Class<?> pType) {
        return oType == pType || (pType.isPrimitive() && (
                (oType == Integer.class && pType == int.class)
                || (oType == Long.class && pType == long.class)
                || (oType == Short.class && pType == short.class)
                || (oType == Byte.class && pType == byte.class)
                || (oType == Boolean.class && pType == boolean.class)
                || (oType == Character.class && pType == char.class)
                || (oType == Double.class && pType == double.class)
                || (oType == Float.class && pType == float.class)));
    }

    private static boolean isPrimitiveBox(Class<?> type) {
        return type == Integer.class || type == Long.class || type == Short.class || type == Byte.class
                || type == Character.class || type == Double.class || type == Float.class;
    }

    private static Class<?> boxType(Class<?> primitiveType) {
        if (primitiveType == int.class) return Integer.class;
        if (primitiveType == long.class) return Long.class;
        if (primitiveType == short.class) return Short.class;
        if (primitiveType == byte.class) return Byte.class;
        if (primitiveType == char.class) return Character.class;
        if (primitiveType == double.class) return Double.class;
        if (primitiveType == float.class) return Float.class;
        throw new IllegalArgumentException("not primitive type: " + primitiveType);
    }

    private static Class<?> primitiveType(Class<?> boxType) {
        if (boxType == Integer.class) return int.class;
        if (boxType == Long.class) return long.class;
        if (boxType == Short.class) return short.class;
        if (boxType == Byte.class) return byte.class;
        if (boxType == Character.class) return char.class;
        if (boxType == Double.class) return double.class;
        if (boxType == Float.class) return float.class;
        throw new IllegalArgumentException("not box type: " + boxType);
    }

    private static boolean typeAssignable(Class<?> oType, Class<?> pType) {
        return pType.isAssignableFrom(oType) || (pType.isPrimitive() && isPrimitiveBox(oType));
    }

    private static Object primitiveCast(Object num, Class<?> numType) {
        var boxType = numType;
        if (boxType.isPrimitive()) boxType = boxType(boxType);
        if (num.getClass() == boxType) return num;
        Number n;
        if (num instanceof Character) {
            n = (int) (Character) num;
        } else {
            n = (Number) num;
        }
        if (boxType == Integer.class) {
            return n.intValue();
        } else if (boxType == Long.class) {
            return n.longValue();
        } else if (boxType == Byte.class) {
            return n.byteValue();
        } else if (boxType == Float.class) {
            return n.floatValue();
        } else if (boxType == Double.class) {
            return n.doubleValue();
        } else if (boxType == Short.class) {
            return n.shortValue();
        } else if (boxType == Character.class) {
            return (char) n.intValue();
        }
    }

    private static Method findMethodBestMatch(Method[] methods, boolean isStatic, Object ...args) {
        Method candidate = null;
        for (var m: methods) {
            if (isStatic != Modifier.isStatic(m.getModifiers())) continue;
            var types = m.getParameterTypes();
            boolean fullMatch = true, match = true, isVarArgs = m.isVarArgs();
            if (!isVarArgs && types.length != args.length) continue;
            var len = types.length + (isVarArgs ? -1 : 0);
            if (isVarArgs && len > args.length) continue;
            assert len >= 0;
            for (int i = 0; i < len; i++) {
                var arg = args[i];
                var pType = types[i];
                var argType = arg == null ? Object.class : arg.getClass();
                if (!typeEquals(argType, pType)) {
                    fullMatch = false;
                }
                if (!typeAssignable(argType, pType)) {
                    match = false;
                    break;
                }
            }
            if (!match) continue;
            if (isVarArgs) {
                var varArgType = types[len];
                var cType = varArgType.getComponentType();
                assert cType != null;
                var arrayAsArg = false;
                if (args.length == len + 1) {
                    var arg = args[len];
                    if (arg != null) {
                        var argClass = arg.getClass();
                        arrayAsArg = varArgType.isAssignableFrom(argClass);
                    }
                }
                if (!arrayAsArg) {
                    for (int i = len; i < args.length; i++) {
                        var arg = args[i];
                        var argType = arg == null ? Object.class : arg.getClass();
                        if (!typeEquals(argType, cType)) {
                            fullMatch = false;
                        }
                        if (!typeAssignable(argType, cType)) {
                            match = false;
                            break;
                        }
                    }
                }
            }
            if (fullMatch) return m;
            if (match) candidate = m;
        }
        return candidate;
    }

    void xx(Object[] ...a) {
    }

    void yy() {
        xx((Object[]) new Object[][]{});
        Object[] o = new Integer[]{};
    }

    interface A {
        default int x() {
            return 1;
        }
        int y();
    }

    static class B implements A {
        @Override
        public int x() {
            return 2;
        }

        @Override
        public int y() {
            return 3;
        }
    }

    static A a = new B();*/
}
