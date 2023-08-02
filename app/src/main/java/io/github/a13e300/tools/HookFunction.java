package io.github.a13e300.tools;

import android.app.AndroidAppHelper;
import android.util.Log;

import com.facebook.stetho.rhino.JsConsole;

import org.mozilla.javascript.BaseFunction;
import org.mozilla.javascript.Context;
import org.mozilla.javascript.Function;
import org.mozilla.javascript.NativeJavaClass;
import org.mozilla.javascript.ScriptRuntime;
import org.mozilla.javascript.Scriptable;
import org.mozilla.javascript.Wrapper;
import org.mozilla.javascript.annotations.JSFunction;

import java.lang.reflect.Constructor;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;
import java.util.stream.Collectors;

import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XC_MethodReplacement;
import de.robv.android.xposed.XposedBridge;

public class HookFunction extends BaseFunction {
    private ClassLoader mClassLoader;

    private final ConcurrentHashMap<Member, XC_MethodHook.Unhook> mHooks = new ConcurrentHashMap<>();

    public HookFunction() {}

    public HookFunction(Scriptable scope) {
        setParentScope(scope);
        Object ctor = ScriptRuntime.getTopLevelProp(scope, getClassName());
        if (ctor instanceof Scriptable) {
            Scriptable scriptable = (Scriptable) ctor;
            setPrototype((Scriptable) scriptable.get("prototype", scriptable));
        }
    }

    @Override
    public String getClassName() {
        return "HookFunction";
    }

    private synchronized ClassLoader getClassLoader() {
        if (mClassLoader == null) {
            mClassLoader = AndroidAppHelper.currentApplication().getClassLoader();
        }
        return mClassLoader;
    }

    @Override
    public Object call(Context cx, Scriptable scope, Scriptable thisObj, Object[] args) {
        if (args.length == 0) return null;
        var first = args[0];
        Class<?> hookClass = null;
        List<Member> hookMembers = null;
        Function callback;
        int pos = 0;
        if (first instanceof Class) {
            hookClass = (Class<?>) first;
        } else if (first instanceof NativeJavaClass) {
            hookClass = ((NativeJavaClass) first).getClassObject();
        } else if (first instanceof ClassLoader) {
            if (args.length >= 2 && args[1] instanceof String) {
                pos++;
                try {
                    hookClass = ((ClassLoader) first).loadClass((String) args[1]);
                } catch (ClassNotFoundException e) {
                    throw new RuntimeException(e);
                }
            } else {
                throw new IllegalArgumentException("classloader should follow a class name");
            }
        } else if (first instanceof String) {
            try {
                getClassLoader().loadClass((String) first);
            } catch (ClassNotFoundException e) {
                throw new RuntimeException(e);
            }
        } else if (first instanceof Method || first instanceof Constructor) {
            hookMembers = List.of((Member) first);
        }
        if (hookMembers == null && hookClass != null) {
            pos++;
            String methodName;
            boolean emptySig = false;
            if (args[pos] instanceof String) {
                methodName = (String) args[pos];
                // methodName, null, callback -> only hook methodName and argument list is empty
                if (pos + 1 == args.length - 2 && args[pos + 1] == null) {
                    emptySig = true;
                }
            } else throw new IllegalArgumentException("method name is required");
            pos++;
            Class<?>[] methodSig;
            if (!emptySig) {
                int nArgs = args.length - 1 - pos;
                methodSig = new Class[nArgs];
                if (nArgs > 0) {
                    for (int i = pos; i < args.length - 1; i++) {
                        if (!(args[i] instanceof Class)) {
                            throw new IllegalArgumentException("argument " + i + " is not class");
                        }
                    }
                    System.arraycopy(args, pos + 2, methodSig, 0, nArgs);
                }
            } else {
                methodSig = new Class[0];
            }
            try {
                if ("<init>".equals(methodName)) {
                    if (methodSig.length == 0 && !emptySig) {
                        hookMembers = List.of(hookClass.getDeclaredConstructors());
                    } else {
                        hookMembers = List.of(hookClass.getDeclaredConstructor(methodSig));
                    }
                } else {
                    if (methodSig.length != 0 || emptySig) {
                        hookMembers = List.of(hookClass.getDeclaredMethod(methodName, methodSig));
                    } else {
                        hookMembers = Arrays.stream(hookClass.getDeclaredMethods()).filter((it) -> methodName.equals(it.getName())).collect(Collectors.toList());
                    }
                }
            } catch (NoSuchMethodException e) {
                throw new RuntimeException(e);
            }
        }
        if (hookMembers == null) {
            throw new IllegalArgumentException("invalid input");
        }
        if (!(args[args.length - 1] instanceof Function)) throw new IllegalArgumentException("callback is required");
        callback = (Function) args[args.length - 1];
        var methodHook = new XC_MethodReplacement() {
            @Override
            protected Object replaceHookedMethod(MethodHookParam param) throws Throwable {
                var p = new HookParam(scope);
                p.setParam(param);
                var context = enterJsContext();
                try {
                    callback.call(context, scope, null, new Object[]{p});
                } catch (Throwable t) {
                    JsConsole.fromScope(scope).error("error occurred in hook of " + param.method, t);
                } finally {
                    Context.exit();
                }
                if (!p.isInvoked()) return p.invoke();
                else {
                    if (p.mParam.hasThrowable()) throw p.mParam.getThrowable();
                    else return p.mParam.getResult();
                }
            }
        };
        var console = JsConsole.fromScope(scope);
        var unhooks = new ArrayList<XC_MethodHook.Unhook>();
        hookMembers.forEach((member) -> {
            var oldHook = mHooks.get(member);
            if (oldHook != null) {
                console.log("remove old hook of", member);
                oldHook.unhook();
            }
            var unhook = XposedBridge.hookMethod(member, methodHook);
            unhooks.add(unhook);
            mHooks.put(member, unhook);
            console.log("hooked", member);
        });
        var unhook = new UnhookFunction(scope);
        unhook.setUnhooks(unhooks);
        return unhook;
    }

    public void clearHooks() {
        Log.d("HookFunction", "cleaning all hooks");
        for (var key: mHooks.keySet()) {
            var value = mHooks.get(key);
            if (value != null) {
                value.unhook();
                Log.d("HookFunction", "clear hook of " + key);
            }
        }
    }

    @Override
    public String toString() {
        return "Use hook.help() for help";
    }

    synchronized void stat() {
        var sb = new StringBuilder("current classLoader").append(getClassLoader()).append("\nHooked methods:");
        for (var key: mHooks.keySet()) {
            sb.append("\n");
            sb.append(key);
        }
        JsConsole.fromScope(getParentScope()).log(sb.toString());
    }

    synchronized void setClassLoader(ClassLoader cl) {
        mClassLoader = cl;
    }

    private static final String HELP = "hook([<class>] <method> <callback>) -> Unhook\n"
            + "  Class:\n"
            + "    hook(classLoader, className, ...)\n"
            + "    hook(className, ...)\n use default classLoader (first app classloader, use hook.setClassLoader to change)\n"
            + "    hook(java.lang.Class.forName(...), ...) or hook(Rhino NativeJavaClass, ...)\n"
            + "  Method:\n"
            + "    hook(<class>, methodName[, method argument types ...])\n"
            + "    if no argument types specified, means hook all methods named methodName\n"
            + "    or hook(MethodObject, ...)\n"
            + "  Callback:\n    hook(..., Callback function)\n"
            + "    Callback function is called before original method invoked,\n"
            + "    with a HookParam object. Return value and exception are ignored (exceptions are logged).\n"
            + "    You must use HookParam to modify the arguments and result of the invocation\n"
            + "  HookParam:\n    .method Readonly\n    .thisObject Readonly\n    .args\n    .result\n    .throwable\n"
            + "    .invoke() - invoke the original method (args from HookParam)\n"
            + "    if invoke() is not called during the callback, the original\n"
            + "    method is automatically invoked after the callback\n"
            + "    except result or throwable of the HookParam is set.\n"
            + "  Unhook:\n"
            + "    A call to hook() return an unhook object, call .unhook() to unhook all methods\n"
            + "    If you want to hook a hooked method, the previous hook will be unhook.\n"
            + "    You can also call hook.clearHooks() to clear all hooks, call hook.stat() to print all hooks.\n"
            + "    When you disconnected from the console, all hooks are automatically removed\n"
            + "  Utils:\n"
            + "    getStackTrace()\n"
            + "    printStackTrace()\n";

    @JSFunction
    public static String toString(Context cx, Scriptable thisObj, Object[] args, Function funObj) {
        return thisObj.toString();
    }

    @JSFunction
    public static void clearHooks(Context cx, Scriptable thisObj, Object[] args, Function funObj) {
        ((HookFunction) thisObj).clearHooks();
    }

    @JSFunction
    public static void stat(Context cx, Scriptable thisObj, Object[] args, Function funObj) {
        ((HookFunction) thisObj).stat();
    }

    @JSFunction
    public static void setClassLoader(Context cx, Scriptable thisObj, Object[] args, Function funObj) {
        if (args.length != 1) throw new IllegalArgumentException("classLoader required!");
        var cl = args[0];
        if (cl instanceof Wrapper) {
            cl = ((Wrapper) cl).unwrap();
        }
        if (!(cl instanceof ClassLoader)) throw new IllegalArgumentException("invalid argument");
        ((HookFunction) thisObj).setClassLoader((ClassLoader) cl);
    }

    @JSFunction
    public static void help(Context cx, Scriptable thisObj, Object[] args, Function funObj) {
        JsConsole.fromScope(thisObj.getParentScope()).log(HELP);
    }

    static Context enterJsContext() {
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
