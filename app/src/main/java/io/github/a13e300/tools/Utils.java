package io.github.a13e300.tools;

import android.app.Activity;
import android.app.ActivityThread;

import org.mozilla.javascript.Context;
import org.mozilla.javascript.ScriptableObject;

import java.util.Map;

import de.robv.android.xposed.XposedHelpers;

public class Utils {
    public static ActivityThread getActivityThread() {
        return ActivityThread.currentActivityThread();
    }

    public static String getProcessName() {
        try {
            return getActivityThread().getProcessName();
        } catch (NullPointerException e) {
            return null;
        }
    }

    public static Activity[] getActivities(ScriptableObject ignore) {
        var activities = (Map<?, ?>) XposedHelpers.getObjectField(getActivityThread(), "mActivities");
        return activities.values().stream().map(v -> (Activity) XposedHelpers.getObjectField(v, "activity")).toArray(Activity[]::new);
    }

    public static Activity getCurrentActivity(ScriptableObject ignore) {
        var activities = (Map<?, ?>) XposedHelpers.getObjectField(getActivityThread(), "mActivities");
        var acr = activities.values().stream().filter(v ->
            !XposedHelpers.getBooleanField(v, "paused")
        ).findFirst();
        return acr.map(o -> (Activity) XposedHelpers.getObjectField(o, "activity")).orElse(null);
    }

    public static String getStackTrace(boolean hide) {
        var sb = new StringBuilder();
        var st = Thread.currentThread().getStackTrace();
        for (int i = 2; i < st.length; i++) {
            if (!hide && st[i].getClassName().startsWith("org.mozilla.javascript")) continue;
            sb.append(st[i].toString()).append("\n");
        }
        return sb.toString();
    }

    public static Context enterJsContext() {
        final Context jsContext = Context.enter();

        // If we cause the context to throw a runtime exception from this point
        // we need to make sure that exit the context.
        try {
            jsContext.setLanguageVersion(Context.VERSION_1_8);

            // We can't let Rhino to optimize the JS and to use a JIT because it would generate JVM bytecode
            // and android runs on DEX bytecode. Instead we need to go in interpreted mode.
            jsContext.setOptimizationLevel(-1);
        } catch (RuntimeException e) {
            // Something bad happened to the javascript context but it might still be usable.
            // The first thing to do is to exit the context and then propagate the error.
            Context.exit();
            throw e;
        }

        return jsContext;
    }
}
