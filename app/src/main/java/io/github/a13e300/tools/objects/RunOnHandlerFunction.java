package io.github.a13e300.tools.objects;

import static io.github.a13e300.tools.Utils.enterJsContext;

import android.os.Handler;

import com.facebook.stetho.rhino.JsConsole;

import org.mozilla.javascript.BaseFunction;
import org.mozilla.javascript.Context;
import org.mozilla.javascript.Function;
import org.mozilla.javascript.Scriptable;
import org.mozilla.javascript.Wrapper;

public class RunOnHandlerFunction extends BaseFunction {
    private final Handler mHandler;
    public RunOnHandlerFunction(Handler handler) {
        mHandler = handler;
    }

    public RunOnHandlerFunction() {
        mHandler = null;
    }

    @Override
    public Object call(Context cx, Scriptable scope, Scriptable thisObj, Object[] args) {
        Handler handler;
        if (args.length < 1 || !(args[0] instanceof Function)) {
            throw new IllegalArgumentException("arg0 must be a function");
        }
        handler = mHandler;
        var fn = (Function) args[0];
        if (handler == null && args.length == 2) {
            var arg1 = args[1];
            if (arg1 instanceof Wrapper) arg1 = ((Wrapper) arg1).unwrap();
            if (!(arg1 instanceof Handler)) throw new IllegalArgumentException("arg1 must be a handler");
            handler = (Handler) arg1;
        }
        handler.post(() -> {
            var context = enterJsContext();
            try {
                fn.call(context, scope, null, new Object[0]);
            } catch (Throwable t) {
                JsConsole.fromScope(scope).error("error at handler", t);
            } finally {
                Context.exit();
            }
        });
        return null;
    }
}
