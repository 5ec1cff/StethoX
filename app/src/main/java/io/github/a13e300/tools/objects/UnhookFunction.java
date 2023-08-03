package io.github.a13e300.tools.objects;

import org.mozilla.javascript.Context;
import org.mozilla.javascript.Function;
import org.mozilla.javascript.ScriptRuntime;
import org.mozilla.javascript.Scriptable;
import org.mozilla.javascript.ScriptableObject;
import org.mozilla.javascript.annotations.JSFunction;

import java.util.List;

import de.robv.android.xposed.XC_MethodHook;

public class UnhookFunction extends ScriptableObject {
    public UnhookFunction() {
    }

    private List<XC_MethodHook.Unhook> mUnhooks;

    public UnhookFunction(Scriptable scope) {
        setParentScope(scope);
        Object ctor = ScriptRuntime.getTopLevelProp(scope, getClassName());
        if (ctor instanceof Scriptable) {
            Scriptable scriptable = (Scriptable) ctor;
            setPrototype((Scriptable) scriptable.get("prototype", scriptable));
        }
    }

    void setUnhooks(List<XC_MethodHook.Unhook> unhooks) {
        mUnhooks = unhooks;
    }

    @Override
    public String getClassName() {
        return "UnhookFunction";
    }

    @JSFunction
    public static String toString(Context cx, Scriptable thisObj, Object[] args, Function funObj) {
        var f = (UnhookFunction) thisObj;
        var sb = new StringBuilder("Unhook of ")
                .append(f.mUnhooks.size())
                .append(" methods");
        for (var m: f.mUnhooks) {
            sb.append("\n");
            sb.append(m);
        }
        return sb.toString();
    }

    @JSFunction
    public static void unhook(Context cx, Scriptable thisObj, Object[] args, Function funObj) {
        var f = (UnhookFunction) thisObj;
        for (var m: f.mUnhooks) {
            m.unhook();
        }
        f.mUnhooks = null;
    }
}
