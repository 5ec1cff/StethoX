package io.github.a13e300.tools;

import android.app.Activity;
import android.app.ActivityThread;

import org.mozilla.javascript.Context;
import org.mozilla.javascript.ScriptableObject;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import de.robv.android.xposed.XposedHelpers;
import io.github.a13e300.tools.node.Node;

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

    public static Object getSupportFragmentManager(ScriptableObject ignore) {
        var currentActivity = getCurrentActivity(ignore);
        if (currentActivity == null) return null;

        var clazz = XposedHelpers.findClass("androidx.fragment.app.FragmentActivity", currentActivity.getClassLoader());
        Method method = XposedHelpers.findMethodExactIfExists(clazz, "getSupportFragmentManager");
        try {
            return method.invoke(currentActivity);
        } catch (InvocationTargetException | IllegalAccessException e) {
            return null;
        }
    }

    public static ArrayList<Node<Object>> getFragments(ScriptableObject ignore) {
        Object manager = getSupportFragmentManager(ignore);
        if (manager == null) return null;

        var rootNodes = new ArrayList<Node<Object>>();
        var nodeStack = new ArrayList<List<Node<Object>>>();
        nodeStack.add(rootNodes);

        var fragmentMgrStack = new ArrayList<>();
        fragmentMgrStack.add(manager);

        var loader = manager.getClass().getClassLoader();
        var fragmentMgrClazz = XposedHelpers.findClass("androidx.fragment.app.FragmentManager", loader);
        var fragmentClazz = XposedHelpers.findClass("androidx.fragment.app.Fragment", loader);

        while (!fragmentMgrStack.isEmpty()) {
            Object currentManager = fragmentMgrStack.remove(fragmentMgrStack.size() - 1);
            List<Node<Object>> currentNode = nodeStack.remove(nodeStack.size() - 1);
            // Object currentManager = fragmentMgrStack.remove(0);
            // List<Node<Object>> currentNode = nodeStack.remove(0);

            try {
                Method method = XposedHelpers.findMethodExactIfExists(fragmentMgrClazz, "getFragments");
                var fragments = (List<?>) method.invoke(currentManager);

                for (int i = 0; i < fragments.size(); i++) {
                    Object fragment = fragments.get(i);

                    var childNode = new Node<>();
                    childNode.index = i;
                    childNode.value = fragment;
                    childNode.children = new ArrayList<>();
                    currentNode.add(childNode);

                    Method childMethod = XposedHelpers.findMethodExactIfExists(fragmentClazz, "getChildFragmentManager");
                    Object childManager = childMethod.invoke(fragment);
                    if (childManager == null) continue;

                    fragmentMgrStack.add(childManager);
                    nodeStack.add(childNode.children);
                }

            } catch (InvocationTargetException | IllegalAccessException e) {
                // Ignore: the exception and continue with the next fragment
            }
        }

        return rootNodes;
    }

    public static String getStackTrace(boolean hide) {
        var sb = new StringBuilder();
        var st = Thread.currentThread().getStackTrace();
        for (int i = 3; i < st.length; i++) {
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
