package io.github.a13e300.tools;

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

    public static native void enumerateClassLoader(ObjectEnumerator e);
    public static native boolean initNative();
}
