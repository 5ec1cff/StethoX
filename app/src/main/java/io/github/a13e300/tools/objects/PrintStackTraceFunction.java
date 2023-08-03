package io.github.a13e300.tools.objects;

import com.facebook.stetho.rhino.JsConsole;

import org.mozilla.javascript.BaseFunction;
import org.mozilla.javascript.Context;
import org.mozilla.javascript.Scriptable;

import io.github.a13e300.tools.Utils;

public class PrintStackTraceFunction extends BaseFunction {
    @Override
    public Object call(Context cx, Scriptable scope, Scriptable thisObj, Object[] args) {
        var hide = args.length >= 1 && args[0] == Boolean.TRUE;
        JsConsole.fromScope(scope).log(Utils.getStackTrace(hide));
        return null;
    }
}
