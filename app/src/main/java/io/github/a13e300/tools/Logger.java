package io.github.a13e300.tools;

import android.util.Log;

public class Logger {
    private static final String TAG = "StethoX";
    public static void d(String msg) {
        Log.d(TAG, msg);
    }

    public static void e(String msg) {
        Log.e(TAG, msg);
    }

    public static void e(String msg, Throwable t) {
        Log.e(TAG, msg, t);
    }
}
