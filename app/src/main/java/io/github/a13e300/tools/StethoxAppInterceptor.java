package io.github.a13e300.tools;

import android.annotation.SuppressLint;
import android.app.ActivityThread;
import android.app.Application;
import android.app.IActivityManager;
import android.app.IApplicationThread;
import android.app.Instrumentation;
import android.content.Context;
import android.content.ContextWrapper;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.ServiceManager;
import android.os.SystemProperties;

import com.facebook.stetho.Stetho;
import com.facebook.stetho.rhino.JsRuntimeReplFactoryBuilder;

import org.mozilla.javascript.BaseFunction;
import org.mozilla.javascript.Scriptable;
import org.mozilla.javascript.ScriptableObject;

import java.io.File;
import java.io.FileInputStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

import de.robv.android.xposed.IXposedHookZygoteInit;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;
import io.github.a13e300.tools.objects.FindStackTraceFunction;
import io.github.a13e300.tools.objects.GetStackTraceFunction;
import io.github.a13e300.tools.objects.HookFunction;
import io.github.a13e300.tools.objects.HookParam;
import io.github.a13e300.tools.objects.JArrayFunction;
import io.github.a13e300.tools.objects.OkHttpInterceptorObject;
import io.github.a13e300.tools.objects.PrintStackTraceFunction;
import io.github.a13e300.tools.objects.RunOnHandlerFunction;
import io.github.a13e300.tools.objects.UnhookFunction;

public class StethoxAppInterceptor implements IXposedHookZygoteInit {

    private boolean initialized = false;
    private boolean mWaiting = false;
    public static ClassLoader mClassLoader = null;

    private synchronized void cont(ScriptableObject ignore) {
        if (mWaiting) {
            notify();
            Stetho.setSuspend(false);
        }
    }

    private synchronized void initializeStetho(Context context) throws InterruptedException {
        if (initialized) return;
        var packageName = context.getPackageName();
        var a = (IApplicationThread) ActivityThread.currentActivityThread().getApplicationThread();
        var am = IActivityManager.Stub.asInterface(ServiceManager.getService("activity"));
        if (BuildConfig.APPLICATION_ID.equals(packageName) || BuildConfig.APPLICATION_ID.equals(Utils.getProcessName())) {
            initialized = true;
            return;
        }

        var processName = Utils.getProcessName();
        Logger.d("Install Stetho: " + packageName + " proc=" + processName);
        var suspendRequested = false;

        try {
            var packages = SystemProperties.get("debug.stethox.suspend");
            if (Arrays.asList(packages.split(",")).contains(processName)) {
                suspendRequested = true;
                Logger.d("suspend requested b prop");
            }
        } catch (Throwable t) {
            Logger.e("get suspend prop", t);
        }

        if (!suspendRequested) {
            try {
                var file = new File("/data/local/tmp/stethox_suspend");
                if (file.canRead()) {
                    var bytes = new byte[(int) file.length()];
                    try (var is = new FileInputStream(file)) {
                        is.read(bytes);
                    }
                    var str = new String(bytes, StandardCharsets.UTF_8);
                    if (Arrays.asList(str.split("\n")).contains(processName)) {
                        suspendRequested = true;
                        Logger.d("suspend requested by file");
                    }
                }
            } catch (Throwable t) {
                Logger.e("get suspend file", t);
            }
        }

        if (!suspendRequested) {
            try {
                var r = context.getContentResolver().call(
                        Uri.parse("content://io.github.a13e300.tools.stethox.suspend"),
                        "process_started", processName, null
                );
                if (r != null && r.getBoolean("suspend", false)) {
                    suspendRequested = true;
                    Logger.d("suspend requested by provider");
                }
            } catch (IllegalArgumentException e) {
                Logger.e("failed to find suspend provider, please ensure module process without restriction", e);
            }
        }

        if (suspendRequested) {
            mWaiting = true;
            Stetho.setSuspend(true);
        }

        try {
            mClassLoader = context.getClassLoader();
        } catch (Throwable t) {
            Logger.e("failed to get classloader for context", t);
        }
        Stetho.initialize(Stetho.newInitializerBuilder(context)
                .enableWebKitInspector(
                        () -> new Stetho.DefaultInspectorModulesBuilder(context).runtimeRepl(
                                new JsRuntimeReplFactoryBuilder(context)
                                        .addFunction("getStackTrace", new GetStackTraceFunction())
                                        .addFunction("printStackTrace", new PrintStackTraceFunction())
                                        .addFunction("findStackTrace", new FindStackTraceFunction())
                                        .addFunction("runOnUiThread", new RunOnHandlerFunction(new Handler(Looper.getMainLooper())))
                                        .addFunction("runOnHandler", new RunOnHandlerFunction())
                                        .addFunction("JArray", new JArrayFunction(null))
                                        .addFunction("IntArray", new JArrayFunction(int.class))
                                        .addFunction("LongArray", new JArrayFunction(long.class))
                                        .addFunction("ByteArray", new JArrayFunction(byte.class))
                                        .addFunction("BooleanArray", new JArrayFunction(boolean.class))
                                        .addFunction("CharArray", new JArrayFunction(char.class))
                                        .addFunction("FloatArray", new JArrayFunction(float.class))
                                        .addFunction("DoubleArray", new JArrayFunction(double.class))
                                        .addFunction("ShortArray", new JArrayFunction(short.class))
                                        .addVariable("int", int.class)
                                        .addVariable("long", long.class)
                                        .addVariable("double", double.class)
                                        .addVariable("float", float.class)
                                        .addVariable("byte", byte.class)
                                        .addVariable("boolean", boolean.class)
                                        .addVariable("char", char.class)
                                        .addVariable("short", short.class)
                                        .importPackage("java.lang")
                                        .importClass(NativeUtils.class)
                                        .addVariable("xposed_bridge", XposedBridge.class)
                                        .addVariable("xposed_helper", XposedHelpers.class)
                                        .onInitScope((jsContext, scope) -> {
                                            try {
                                                scope.defineProperty("activities", null, Utils.class.getDeclaredMethod("getActivities", ScriptableObject.class), null, ScriptableObject.READONLY);
                                                scope.defineProperty("current", null, Utils.class.getDeclaredMethod("getCurrentActivity", ScriptableObject.class), null, ScriptableObject.READONLY);
                                                scope.defineProperty("fragments", null, Utils.class.getDeclaredMethod("getFragments", ScriptableObject.class), null, ScriptableObject.READONLY);
                                                ScriptableObject.defineClass(scope, HookFunction.class);
                                                ScriptableObject.defineClass(scope, UnhookFunction.class);
                                                ScriptableObject.defineClass(scope, HookParam.class);
                                                ScriptableObject.defineClass(scope, OkHttpInterceptorObject.class);
                                                scope.defineProperty("hook", new HookFunction(scope), ScriptableObject.DONTENUM | ScriptableObject.CONST);
                                                scope.defineProperty("okhttp3", new OkHttpInterceptorObject(scope), ScriptableObject.DONTENUM | ScriptableObject.CONST);
                                                synchronized (StethoxAppInterceptor.this) {
                                                    if (mWaiting) {
                                                        var method = StethoxAppInterceptor.class.getDeclaredMethod("cont", ScriptableObject.class);
                                                        method.setAccessible(true);
                                                        scope.defineProperty("cont", StethoxAppInterceptor.this, method, null, ScriptableObject.READONLY);
                                                    }
                                                }
                                                jsContext.evaluateString(scope,
                                                        "let st, pst, fst; st = pst = printStackTrace; fst = findStackTrace;"
                                                        + "intv=java.lang.Integer.valueOf; longv=java.lang.Long.valueOf; "
                                                        + "boolv=java.lang.Boolean.valueOf; bytev=java.lang.Byte.valueOf; "
                                                        + "charv=java.lang.Char.valueOf; shortv=java.lang.Short.valueOf; "
                                                        + "floatv=java.lang.Float.valueOf; doublev=java.lang.Double.valueOf; ",
                                                        "initializer", 1, null);
                                            } catch (NoSuchMethodException |
                                                     InvocationTargetException |
                                                     IllegalAccessException |
                                                     InstantiationException e) {
                                                throw new RuntimeException(e);
                                            }
                                        })
                                        .onFinalize(scope -> {
                                            ((HookFunction) ScriptableObject.getProperty(scope, "hook")).clearHooks();
                                            ((OkHttpInterceptorObject) ScriptableObject.getProperty(scope, "okhttp3")).stop(true);
                                        })
                                        // .importClass(DexUtils.class)
                                        // .importClass(StethoOkHttp3ProxyInterceptor.class)
                                        // .importClass(MutableNameMap.class)
                                        // .importClass(DexKitWrapper.class)
                                        // .importPackage("org.luckypray.dexkit.query")
                                        // .importPackage("org.luckypray.dexkit.query.matchers")
                                        .addFunction("MYCL", new BaseFunction() {
                                            @Override
                                            public Object call(org.mozilla.javascript.Context cx, Scriptable scope, Scriptable thisObj, Object[] args) {
                                                return StethoxAppInterceptor.class.getClassLoader();
                                            }
                                        })
                                        .build()
                        ).finish()
                ).build()
        );
        initialized = true;
        if (mWaiting) {
            try {
                am.showWaitingForDebugger(a, true);
            } catch (Throwable t) {
                Logger.e("failed to show wait for debugger", t);
            }
            wait();
            mWaiting = false;
            try {
                am.showWaitingForDebugger(a, false);
            } catch (Throwable t) {
                Logger.e("failed to close wait for debugger", t);
            }
        }
    }

    @SuppressLint("PrivateApi")
    @Override
    public void initZygote(StartupParam startupParam) throws Throwable {
        XposedBridge.log("initZygote");
        // initWithMakeApplication();
        initWithAttach();
    }

    private void initWithAttachBaseContext() {
        XposedHelpers.findAndHookMethod(ContextWrapper.class, "attachBaseContext", Context.class, new XC_MethodHook() {
            @Override
            protected void afterHookedMethod(MethodHookParam param) throws Throwable {
                if (!(param.thisObject instanceof Application)) return;
                var context = (Application) param.thisObject;
                XposedBridge.log("context " + context);
                if (context == null) return;
                initializeStetho(context);
            }
        });
    }

    private void initWithAttach() {
        XposedHelpers.findAndHookMethod(Application.class, "attach", Context.class, new XC_MethodHook() {
            @Override
            protected void afterHookedMethod(MethodHookParam param) throws Throwable {
                XposedBridge.log("after attach " + param.thisObject);
                var context = (Application) param.thisObject;
                XposedBridge.log("context " + context);
                if (context == null) return;
                initializeStetho(context);
            }
        });
    }

    private void initWithMakeApplication() {
        Method makeApplication;
        try {
            makeApplication = XposedHelpers.findMethodExact(
                    "android.app.LoadedApk",
                    ClassLoader.getSystemClassLoader(),
                    "makeApplicationInner",
                    boolean.class,
                    Instrumentation.class
            );
        } catch (NoSuchMethodError e) {
            makeApplication = XposedHelpers.findMethodExact(
                    "android.app.LoadedApk",
                    ClassLoader.getSystemClassLoader(),
                    "makeApplication",
                    boolean.class,
                    Instrumentation.class
            );
        }
        XposedBridge.hookMethod(makeApplication,
                new XC_MethodHook() {
                    @Override
                    protected void afterHookedMethod(MethodHookParam param) throws Throwable {
                        var context = (Application) param.getResult();
                        XposedBridge.log("context " + context);
                        if (context == null) return;
                        initializeStetho(context);
                    }
                }
        );
    }
}
