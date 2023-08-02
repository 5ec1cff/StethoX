package io.github.a13e300.tools;

import org.mozilla.javascript.Context;
import org.mozilla.javascript.Function;
import org.mozilla.javascript.ScriptRuntime;
import org.mozilla.javascript.Scriptable;
import org.mozilla.javascript.ScriptableObject;
import org.mozilla.javascript.annotations.JSFunction;
import org.mozilla.javascript.annotations.JSGetter;
import org.mozilla.javascript.annotations.JSSetter;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Member;

import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;

public class HookParam extends ScriptableObject {
    XC_MethodHook.MethodHookParam mParam;
    private boolean mInvoked = false;

    public HookParam() {}

    public HookParam(Scriptable scope) {
        setParentScope(scope);
        Object ctor = ScriptRuntime.getTopLevelProp(scope, getClassName());
        if (ctor instanceof Scriptable) {
            Scriptable scriptable = (Scriptable) ctor;
            setPrototype((Scriptable) scriptable.get("prototype", scriptable));
        }
    }

    void setParam(XC_MethodHook.MethodHookParam param) {
        mParam = param;
    }

    synchronized Object invoke() throws Throwable {
        Object mOriginalResult;
        try {
            mOriginalResult = XposedBridge.invokeOriginalMethod(mParam.method, mParam.thisObject, mParam.args);
            if (!mInvoked) mParam.setResult(mOriginalResult);
        } catch (Throwable t) {
            var realT = t;
            if (realT instanceof InvocationTargetException) realT = ((InvocationTargetException) realT).getTargetException();
            if (!mInvoked) mParam.setThrowable(realT);
            throw realT;
        } finally {
            mInvoked = true;
        }
        return mOriginalResult;
    }

    @JSFunction
    public static Object invoke(Context cx, Scriptable thisObj, Object[] args, Function funObj) throws Throwable {
        var f = (HookParam) thisObj;
        return f.invoke();
    }

    boolean isInvoked() {
        return mInvoked;
    }

    @Override
    public String getClassName() {
        return "HookParam";
    }

    @JSGetter
    public static Object getResult(Scriptable thisObj) {
        return ((HookParam) thisObj).mParam.getResult();
    }

    @JSSetter
    public static void setResult(Scriptable thisObj, Object value) {
        var p = (HookParam) thisObj;
        p.mParam.setResult(value);
        p.mInvoked = true;
    }

    @JSGetter
    public static Throwable getThrowable(Scriptable thisObj) {
        return ((HookParam) thisObj).mParam.getThrowable();
    }

    @JSSetter
    public static void setThrowable(Scriptable thisObj, Throwable value) {
        var p = (HookParam) thisObj;
        p.mParam.setThrowable(value);
        p.mInvoked = true;
    }

    @JSGetter
    public static Object[] getArgs(Scriptable thisObj) {
        return ((HookParam) thisObj).mParam.args;
    }

    @JSSetter
    public static void setArgs(Scriptable thisObj, Object[] value) {
        ((HookParam) thisObj).mParam.args = value;
    }

    @JSGetter
    public static Object getThisObject(Scriptable thisObj) {
        return ((HookParam) thisObj).mParam.thisObject;
    }

    @JSGetter
    public static Member getMethod(Scriptable thisObj) {
        return ((HookParam) thisObj).mParam.method;
    }
}
