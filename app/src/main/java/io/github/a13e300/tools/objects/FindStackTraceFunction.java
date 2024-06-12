package io.github.a13e300.tools.objects;

import android.app.Activity;
import android.util.Log;
import android.view.View;
import android.view.Window;

import com.facebook.stetho.rhino.JsConsole;

import org.mozilla.javascript.BaseFunction;
import org.mozilla.javascript.Context;
import org.mozilla.javascript.Scriptable;
import org.mozilla.javascript.Wrapper;

import de.robv.android.xposed.XposedHelpers;

public class FindStackTraceFunction extends BaseFunction {
    @Override
    public Object call(Context cx, Scriptable scope, Scriptable thisObj, Object[] args) {
        if (args.length == 1) {
            var obj = args[0];
            Throwable t;
            if (obj instanceof Wrapper) {
                obj = ((Wrapper) obj).unwrap();
            }
            if (obj instanceof Throwable) {
                t = (Throwable) obj;
            } else {
                if (obj instanceof Activity) {
                    obj = ((Activity) obj).getWindow();
                }
                if (obj instanceof Window) {
                    obj = ((Window) obj).getDecorView();
                }
                if (obj instanceof View) {
                    t = (Throwable) XposedHelpers.getObjectField(
                            XposedHelpers.callMethod(obj, "getViewRootImpl"),
                            "mLocation"
                    );
                } else {
                    throw new IllegalArgumentException("required a view or throwable!");
                }
            }
            JsConsole.fromScope(scope).log(Log.getStackTraceString(t));
            return null;
        }
        throw new IllegalArgumentException("must be 1 view or throwable arg !");
    }
}
