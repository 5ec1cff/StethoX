package io.github.a13e300.tools;

import android.annotation.SuppressLint;
import android.app.ActivityThread;
import android.app.Application;
import android.app.IActivityManager;
import android.app.IApplicationThread;
import android.app.Instrumentation;
import android.content.Context;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.ServiceManager;

import com.facebook.stetho.Stetho;
import com.facebook.stetho.rhino.JsRuntimeReplFactoryBuilder;

import org.mozilla.javascript.ScriptableObject;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import de.robv.android.xposed.IXposedHookZygoteInit;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;
import io.github.a13e300.tools.objects.GetStackTraceFunction;
import io.github.a13e300.tools.objects.HookFunction;
import io.github.a13e300.tools.objects.HookParam;
import io.github.a13e300.tools.objects.PrintStackTraceFunction;
import io.github.a13e300.tools.objects.RunOnHandlerFunction;
import io.github.a13e300.tools.objects.UnhookFunction;

public class StethoxAppInterceptor implements IXposedHookZygoteInit {
    private boolean initialized = false;
    private boolean mWaiting = false;

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
        Logger.d("Install Stetho: " + packageName);
        try {
            var r = context.getContentResolver().call(
                    Uri.parse("content://io.github.a13e300.tools.stethox.suspend"),
                    "process_started", Utils.getProcessName(), null
            );
            if (r != null && r.getBoolean("suspend", false)) {
                Logger.d("suspend requested");
                mWaiting = true;
                Stetho.setSuspend(true);
            }
        } catch (IllegalArgumentException e) {
            Logger.e("failed to find suspend provider, please ensure module process without restriction", e);
        }
        Stetho.initialize(Stetho.newInitializerBuilder(context)
                .enableWebKitInspector(
                        () -> new Stetho.DefaultInspectorModulesBuilder(context).runtimeRepl(
                                new JsRuntimeReplFactoryBuilder(context)
                                        .addFunction("getStackTrace", new GetStackTraceFunction())
                                        .addFunction("printStackTrace", new PrintStackTraceFunction())
                                        .addFunction("runOnUiThread", new RunOnHandlerFunction(new Handler(Looper.getMainLooper())))
                                        .addFunction("runOnHandler", new RunOnHandlerFunction())
                                        .onInitScope(scope -> {
                                            try {
                                                scope.defineProperty("activities", null, Utils.class.getDeclaredMethod("getActivities", ScriptableObject.class), null, ScriptableObject.READONLY);
                                                scope.defineProperty("current", null, Utils.class.getDeclaredMethod("getCurrentActivity", ScriptableObject.class), null, ScriptableObject.READONLY);
                                                ScriptableObject.defineClass(scope, HookFunction.class);
                                                ScriptableObject.defineClass(scope, UnhookFunction.class);
                                                ScriptableObject.defineClass(scope, HookParam.class);
                                                scope.defineProperty("hook", new HookFunction(scope), ScriptableObject.DONTENUM | ScriptableObject.CONST);
                                                synchronized (StethoxAppInterceptor.this) {
                                                    if (mWaiting) {
                                                        var method = StethoxAppInterceptor.class.getDeclaredMethod("cont", ScriptableObject.class);
                                                        method.setAccessible(true);
                                                        scope.defineProperty("cont", StethoxAppInterceptor.this, method, null, ScriptableObject.READONLY);
                                                    }
                                                }
                                            } catch (NoSuchMethodException |
                                                     InvocationTargetException |
                                                     IllegalAccessException |
                                                     InstantiationException e) {
                                                throw new RuntimeException(e);
                                            }
                                        })
                                        .onFinalize(scope -> {
                                            ((HookFunction) ScriptableObject.getProperty(scope, "hook")).clearHooks();
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
