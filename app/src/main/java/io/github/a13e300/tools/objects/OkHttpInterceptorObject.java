package io.github.a13e300.tools.objects;

import com.facebook.stetho.rhino.JsConsole;

import org.mozilla.javascript.Context;
import org.mozilla.javascript.Function;
import org.mozilla.javascript.ScriptRuntime;
import org.mozilla.javascript.Scriptable;
import org.mozilla.javascript.ScriptableObject;
import org.mozilla.javascript.Wrapper;
import org.mozilla.javascript.annotations.JSFunction;

import java.util.Set;

import de.robv.android.xposed.XC_MethodHook;
import io.github.a13e300.tools.Logger;
import io.github.a13e300.tools.network.okhttp3.StethoOkHttp3ProxyInterceptor;

public class OkHttpInterceptorObject extends ScriptableObject {
    private Set<XC_MethodHook.Unhook> unhook = null;

    public OkHttpInterceptorObject() {}

    public OkHttpInterceptorObject(Scriptable scope) {
        setParentScope(scope);
        Object ctor = ScriptRuntime.getTopLevelProp(scope, getClassName());
        if (ctor instanceof Scriptable) {
            Scriptable scriptable = (Scriptable) ctor;
            setPrototype((Scriptable) scriptable.get("prototype", scriptable));
        }
    }

    @Override
    public String getClassName() {
        return "OkHttpInterceptorObject";
    }

    private static final int USE_DEOBF_AUTO = 0;
    private static final int USE_DEOBF_YES = 1;
    private static final int USE_DEOBF_NO = 2;

    public synchronized void start(ClassLoader classLoader, int useDeObf) {
        if (unhook == null) {
            var console = JsConsole.fromScope(getParentScope());
            if (useDeObf == USE_DEOBF_AUTO) {
                try {
                    unhook = StethoOkHttp3ProxyInterceptor.start(classLoader, false);
                } catch (Throwable t) {
                    Logger.e("Okhttp3Interceptor start with useDeObf=false failed, try useDeObf=true", t);
                    console.log("Okhttp3Interceptor start with useDeObf=false failed, try useDeObf=true");
                    unhook = StethoOkHttp3ProxyInterceptor.start(classLoader, true);
                }
            } else {
                unhook = StethoOkHttp3ProxyInterceptor.start(classLoader, useDeObf == USE_DEOBF_YES);
            }
            Logger.d("OkHttp3Interceptor started!");
            console.log("Okhttp3Interceptor started!");
        } else {
            throw new IllegalStateException("Okhttp3Interceptor has already started!");
        }
    }

    public synchronized void stop(boolean finalize) {
        if (unhook != null) {
            for (var h: unhook) {
                h.unhook();
            }
            unhook = null;
            Logger.d("OkHttp3Interceptor stopped!");
            if (!finalize) {
                JsConsole.fromScope(getParentScope()).log("Okhttp3Interceptor stopped!");
            }
        } else if (!finalize) {
            throw new IllegalStateException("Okhttp3Interceptor has not yet started!");
        }
    }

    private static void usage() {
        throw new IllegalArgumentException("usage: start([useObf] [, classLoader])");
    }

    @JSFunction
    public static void start(Context cx, Scriptable thisObj, Object[] args, Function funObj) throws Throwable {
        var obj = (OkHttpInterceptorObject) thisObj;
        var hook = (HookFunction) ScriptableObject.getProperty(obj.getParentScope(), "hook");
        ClassLoader cl = null;
        int useDeObf;
        if (args.length == 0) {
            cl = hook.getClassLoader();
            useDeObf = USE_DEOBF_AUTO;
        } else {
            if (!(args[0] instanceof Boolean)) usage();
            useDeObf = ((Boolean) args[0]) ? USE_DEOBF_YES : USE_DEOBF_NO;
            if (args.length == 2) {
                var c = args[1];
                if (c instanceof Wrapper) c = ((Wrapper) c).unwrap();
                if (c instanceof ClassLoader) cl = (ClassLoader) c;
                else usage();
            } else usage();
        }
        obj.start(cl, useDeObf);
    }

    @JSFunction
    public static void stop(Context cx, Scriptable thisObj, Object[] args, Function funObj) throws Throwable {
        var obj = (OkHttpInterceptorObject) thisObj;
        obj.stop(false);
    }

}
