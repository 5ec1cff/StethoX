package io.github.a13e300.tools;


import android.util.Log;

import com.android.dx.DexMaker;
import com.android.dx.TypeId;

import java.lang.reflect.Modifier;
import java.nio.ByteBuffer;

import dalvik.system.InMemoryDexClassLoader;

public class DexUtils {
    public static void log(String msg) {
        Logger.d("DexUtils log: " + msg);
    }
    public static Object make(ClassLoader classLoader, String name, String superName) {
        var maker = new DexMaker();
        var superType = TypeId.get(superName); // LNeverCall$AB;
        var myType = TypeId.get(name); // LMyAB;
        maker.declare(myType, "Generated", Modifier.PUBLIC, superType);
        var printMethod = myType.getMethod(TypeId.VOID, "print", TypeId.STRING);
        var cstr = maker.declare(myType.getConstructor(), Modifier.PUBLIC);
        var superCstr = superType.getConstructor();
        cstr.invokeDirect(superCstr, null, cstr.getThis(myType));
        cstr.returnVoid();
        var code = maker.declare(printMethod, Modifier.PUBLIC);
        var tag = code.newLocal(TypeId.STRING);
        code.loadConstant(tag, "DexUtils");
        var p0 = code.getParameter(0, TypeId.STRING);
        var logI = TypeId.get(Log.class).getMethod(TypeId.INT, "i", TypeId.STRING, TypeId.STRING);
        var myLog = TypeId.get(DexUtils.class).getMethod(TypeId.VOID, "log", TypeId.STRING);
        code.invokeStatic(logI, null, tag, p0);
        code.invokeStatic(myLog, null, p0);
        code.returnVoid();
        var dex = maker.generate();
        try {
            if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.O) return null;
            var loader = new InMemoryDexClassLoader(ByteBuffer.wrap(dex), new HybridClassLoader(classLoader));
            var clazz = loader.loadClass("MyAB");
            Logger.d("class loader=" + clazz.getClassLoader());
            var superClass = classLoader.loadClass("NeverCall$AB");
            var inst = clazz.newInstance();
            var m = superClass.getDeclaredMethod("call", superClass, String.class);
            m.setAccessible(true);
            m.invoke(null, inst, "test");
            return inst;
        } catch (Throwable t) {
            Logger.e("dexutils load and call", t);
        }
        return null;
    }

    static class HybridClassLoader extends ClassLoader {
        private final ClassLoader mBase;
        public HybridClassLoader(ClassLoader parent) {
            mBase = parent;
        }

        @Override
        protected Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
            if (name.startsWith("io.github.a13e300.tools")) {
                return getClass().getClassLoader().loadClass(name);
            }
            try {
                return mBase.loadClass(name);
            } catch (ClassNotFoundException e) {
                return super.loadClass(name, resolve);
            }
        }
    }
}
