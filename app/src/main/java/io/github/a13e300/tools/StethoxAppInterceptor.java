package io.github.a13e300.tools;

import static de.robv.android.xposed.XposedHelpers.findAndHookMethod;

import android.content.Context;

import com.facebook.stetho.Stetho;
import com.facebook.stetho.rhino.JsRuntimeReplFactoryBuilder;

import org.mozilla.javascript.ScriptableObject;

import java.lang.reflect.InvocationTargetException;

import de.robv.android.xposed.IXposedHookLoadPackage;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.callbacks.XC_LoadPackage;

public class StethoxAppInterceptor implements IXposedHookLoadPackage {
    public static final String TAG = "Stethox: ";

    private void initializeStetho(Context context) {
        Stetho.initialize(Stetho.newInitializerBuilder(context)
                .enableWebKitInspector(
                        () -> new Stetho.DefaultInspectorModulesBuilder(context).runtimeRepl(
                                new JsRuntimeReplFactoryBuilder(context)
                                        .onInitScope(scope -> {
                                            try {
                                                scope.defineProperty("activities", null, Utils.class.getDeclaredMethod("getActivities", ScriptableObject.class), null, ScriptableObject.READONLY);
                                                scope.defineProperty("current", null, Utils.class.getDeclaredMethod("getCurrentActivity", ScriptableObject.class), null, ScriptableObject.READONLY);
                                                ScriptableObject.defineClass(scope, HookFunction.class);
                                                ScriptableObject.defineClass(scope, UnhookFunction.class);
                                                ScriptableObject.defineClass(scope, HookParam.class);
                                                scope.defineProperty("hook", new HookFunction(scope), ScriptableObject.DONTENUM | ScriptableObject.CONST);
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
    }


    public void handleLoadPackage(final XC_LoadPackage.LoadPackageParam lpparam) throws Throwable {
        if (lpparam.packageName.equals(BuildConfig.APPLICATION_ID))
            return;

        findAndHookMethod("android.app.Application",
                lpparam.classLoader,
                "onCreate",
                new XC_MethodHook() {
                    @Override
                    protected void afterHookedMethod(MethodHookParam param) {
                        Context context = (Context) param.thisObject;
                        XposedBridge.log(TAG + "Install Stetho: " + context.getPackageName());
                        initializeStetho(context);
                    }
                });
    }
}
