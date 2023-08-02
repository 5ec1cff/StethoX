package io.github.a13e300.tools;

import org.mozilla.javascript.BaseFunction;
import org.mozilla.javascript.Context;
import org.mozilla.javascript.Scriptable;

public class GetStackTraceFunction extends BaseFunction {
    @Override
    public Object call(Context cx, Scriptable scope, Scriptable thisObj, Object[] args) {
        var hide = args.length >= 1 && args[0] == Boolean.TRUE;
        return Utils.getStackTrace(hide);
    }
}
