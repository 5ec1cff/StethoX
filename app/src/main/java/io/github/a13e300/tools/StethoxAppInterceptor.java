package io.github.a13e300.tools;

import android.app.Application;

import com.facebook.stetho.Stetho;

import de.robv.android.xposed.IXposedHookLoadPackage;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.callbacks.XC_LoadPackage;
import static de.robv.android.xposed.XposedHelpers.findAndHookMethod;

public class StethoxAppInterceptor implements IXposedHookLoadPackage {
    public static final String TAG = "Stethox: ";


    public void handleLoadPackage(final XC_LoadPackage.LoadPackageParam lpparam) throws Throwable {
        if(lpparam.packageName.equals(BuildConfig.APPLICATION_ID))
            return;

        // install Stetho
        findAndHookMethod("android.app.Application",
                lpparam.classLoader,
                "onCreate",
                new XC_MethodHook() {
                    @Override
                    protected void afterHookedMethod(MethodHookParam param) {
                        Application app = (Application) param.thisObject;
                        XposedBridge.log(TAG + "Install Stetho: " + app.getPackageName());
                        Stetho.initializeWithDefaults(app);
                    }
                });
    }
}
