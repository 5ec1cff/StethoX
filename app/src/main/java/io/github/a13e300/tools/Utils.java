package io.github.a13e300.tools;

import android.app.Activity;

import org.mozilla.javascript.ScriptableObject;

import java.util.Map;

import de.robv.android.xposed.XposedHelpers;

public class Utils {
    public static Object getActivityThread() {
        return XposedHelpers.callStaticMethod(
                XposedHelpers.findClass("android.app.ActivityThread", Utils.class.getClassLoader()),
                "currentActivityThread"
        );
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
}
