package io.github.a13e300.tools.objects;

import static io.github.a13e300.tools.Utils.enterJsContext;

import android.util.Log;

import com.facebook.stetho.rhino.JsConsole;

import org.mozilla.javascript.BaseFunction;
import org.mozilla.javascript.Context;
import org.mozilla.javascript.Function;
import org.mozilla.javascript.ScriptRuntime;
import org.mozilla.javascript.Scriptable;
import org.mozilla.javascript.Wrapper;
import org.mozilla.javascript.annotations.JSFunction;

import java.lang.reflect.Constructor;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Consumer;
import java.util.stream.Collectors;

import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XC_MethodReplacement;
import de.robv.android.xposed.XposedBridge;
import io.github.a13e300.tools.NativeUtils;
import io.github.a13e300.tools.StethoxAppInterceptor;
import io.github.a13e300.tools.Utils;

public class HookFunction extends BaseFunction {
    /**
     * The classloader of the context
     * DO NOT USE DIRECTLY, use getClassLoader() instead
     */
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

    public synchronized ClassLoader getClassLoader() {
        if (mClassLoader == null) {
            ClassLoader cl = StethoxAppInterceptor.mClassLoader;
            if (cl == null) return ClassLoader.getSystemClassLoader();
            mClassLoader = cl;
        }
        return mClassLoader;
    }

    @Override
    public Object call(Context cx, Scriptable scope, Scriptable thisObj, Object[] args) {
        return hook(cx, scope, args);
    }

    private UnhookFunction hook(Context cx, Scriptable scope, Object[] args) {
        if (args.length == 0) return null;
        var first = args[0];
        if (first instanceof Wrapper) first = ((Wrapper) first).unwrap();
        Class<?> hookClass = null;
        ClassLoader hookClassLoader = getClassLoader();
        List<Member> hookMembers = null;
        Function callback;
        int pos = 0;
        if (first instanceof Class) {
            hookClass = (Class<?>) first;
        } else if (first instanceof ClassLoader) {
            if (args.length >= 2 && args[1] instanceof String) {
                pos++;
                try {
                    hookClassLoader = (ClassLoader) first;
                    hookClass = ((ClassLoader) first).loadClass((String) args[1]);
                } catch (ClassNotFoundException e) {
                    throw new RuntimeException(e);
                }
            } else {
                throw new IllegalArgumentException("classloader should follow a class name");
            }
        } else if (first instanceof String) {
            try {
                hookClass = getClassLoader().loadClass((String) first);
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
            } else {
                methodName = null;
            }
            pos++;
            Class<?>[] methodSig;
            if (!emptySig) {
                int nArgs = args.length - 1 - pos;
                methodSig = new Class[nArgs];
                if (nArgs > 0) {
                    for (int i = 0; i < nArgs; i++) {
                        var arg = args[pos + i];
                        if (arg instanceof Wrapper) arg = ((Wrapper) arg).unwrap();
                        try {
                            if (arg instanceof String)
                                arg = hookClassLoader.loadClass((String) arg);
                        } catch (ClassNotFoundException e) {
                            throw new IllegalArgumentException(e);
                        }
                        if (!(arg instanceof Class)) {
                            throw new IllegalArgumentException("argument " + i + " is not class");
                        }
                        methodSig[i] = (Class<?>) arg;
                    }
                }
            } else {
                methodSig = new Class[0];
            }
            try {
                if (methodName == null) {
                    if (methodSig.length != 0) throw new IllegalArgumentException("should not specify method signature!");
                    hookMembers = List.of(hookClass.getDeclaredMethods());
                } else if (methodName.startsWith("@")) {
                    if (methodName.equals("@")) throw new IllegalArgumentException("must contain at least one of: method,all,ALL,constructor,init,public,instance");
                    var queryArgs = methodName.substring(1).split(",");
                    var isPublic = false;
                    var allMethod = false;
                    var allConstructor = false;
                    var isInstance = false;
                    for (var c: queryArgs) {
                        if ("all".equals(c) || "method".equals(c)) allMethod = true;
                        else if ("ALL".equals(c)) allMethod = allConstructor = true;
                        else if ("constructor".equals(c) || "init".equals(c)) allConstructor = true;
                        else if ("public".equals(c)) allMethod = isPublic = true;
                        else if ("instance".equals(c)) allMethod = isInstance = true;
                        else throw new IllegalArgumentException("must contain at least one of: method,all,ALL,constructor,init,public,instance");
                    }
                    var fIsPublic = isPublic;
                    var fIsInstance = isInstance;
                    if (methodSig.length != 0) throw new IllegalArgumentException("should not specify method signature!");
                    var result = new ArrayList<Member>();
                    Consumer<Member> consumer = (it) -> {
                        var match = true;
                        if (fIsPublic) match = Modifier.isPublic(it.getModifiers());
                        if (fIsInstance) match &= !Modifier.isStatic(it.getModifiers());
                        if (match) result.add(it);
                    };
                    if (allMethod) Arrays.stream(hookClass.getDeclaredMethods()).forEach(consumer);
                    if (allConstructor) Arrays.stream(hookClass.getDeclaredConstructors()).forEach(consumer);
                    hookMembers = result;
                } else if ("<init>".equals(methodName)) {
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
        if (hookMembers == null || hookMembers.isEmpty()) {
            throw new IllegalArgumentException("cannot find members");
        }
        var lastArg = args[args.length - 1];
        XC_MethodHook methodHook;
        if (lastArg instanceof Function) {
            callback = (Function) lastArg;
            methodHook = new XC_MethodReplacement() {
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
        } else if (lastArg instanceof XC_MethodHook) {
            methodHook = (XC_MethodHook) lastArg;
        } else {
            throw new IllegalArgumentException("callback " + lastArg + " is neither Function nor XC_MethodHook");
        }
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
        mHooks.clear();
    }

    @Override
    public String toString() {
        return "Use hook.help() for help";
    }

    Class<?> findClass(Object[] args) throws ClassNotFoundException {
        if (args.length >= 1 && args[0] instanceof String) {
            ClassLoader cl = getClassLoader();
            if (args.length == 2){
                var arg1 = args[1] instanceof Wrapper ? ((Wrapper) args[1]).unwrap() : args[1];
                if (!(arg1 instanceof ClassLoader)) throw new IllegalArgumentException("arg1 must be null or a classloader");
                cl = (ClassLoader) arg1;
            }
            return Class.forName((String) args[0], false, cl);
        } else {
            throw new IllegalArgumentException("arg0 must be a string");
        }
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
            + "  HookParam:\n    .method Readonly\n    .thisObject\n    .args\n    .result\n    .throwable\n"
            + "    .invoke() - invoke the original method (args from HookParam)\n"
            + "    if invoke() is not called during the callback, the original\n"
            + "    method is automatically invoked after the callback\n"
            + "    except result or throwable of the HookParam is set.\n"
            + "  Unhook:\n"
            + "    A call to hook() return an unhook object, call .unhook() to unhook all methods\n"
            + "    If you want to hook a hooked method, the previous hook will be unhook.\n"
            + "    You can also call hook.clear() to clear all hooks, call hook.stat() to print all hooks.\n"
            + "    When you disconnected from the console, all hooks are automatically removed\n"
            + "  Utils:\n"
            + "    packagesCL(): get classloaders of all loaded packages\n"
            + "    usePackage(pkg): use pkg cl\n"
            + "    currentPkg(pkg): get current package of current cl\n"
            + "    getStackTrace()\n"
            + "    printStackTrace() / pst()\n"
            + "    findStackTrace(obj) / fst(obj): find stack trace and print, such as View\n"
            + "    hook.findClass(String[, Classloader])\n"
            + "    hook.setClassLoader / getClassLoader\n"
            + "    hook.getClassLoaders (get an array of all classloaders)\n"
            + "    runOnHandler(callback, handler) & runOnUiThread(callback)\n"
            + "    trace() / traces(): parameters like hook, but without callback, the hook will print corresponding information automatically (traces contains stack trace)\n"
            + "    deoptimizeMethod(method)\n"
            + "    getObjectsOfClass(targetClass[, boolean containsSubClasses]):\n"
            + "      Get all objects in the VM, which class is `targetClass`, which can be a Class Object\n"
            + "      or a String (used for find class in the context's ClassLoader)\n"
            + "      If `containsSubClasses` is true, then the result contains objects which class is\n"
            + "      subclass of `targetClass`. If it is unspecified or false, only objects which class\n"
            + "      is exactly `targetClass` will be returned.\n"
            + "    getAssignableClasses(targetClass[, classLoader])\n"
            + "      Get all classes in the vm, which is assignable to `targetClass`.\n"
            + "      `targetClass` can be a Class object or String, like `getObjectsOfClass`\n"
            + "      `classLoader` can be a ClassLoader object, If it is specified, the the result\n"
            + "      will be filtered out to classes in that classloader.\n"
            + "    importer([classLoader/packageName, ] initialPackage)\n"
            + "      Create an importer for host packages to access their packages conveniently\n"
            + "      For example, if class `com.example.MainActivity` in the host package, \n"
            + "      you can use `importer(com.example)` to get the `com.example` package, or \n"
            + "      use `importer(com.example.MainActivity)` to get the `MainActivity`  \n"
            + "      You can also use rhino's `importPackage` or `importClass` to import them to\n"
            + "      global namespace. For example: importClass(importer().com.example.MainActivity)\n";

    @JSFunction
    public static String toString(Context cx, Scriptable thisObj, Object[] args, Function funObj) {
        return thisObj.toString();
    }

    @JSFunction
    public static void clear(Context cx, Scriptable thisObj, Object[] args, Function funObj) {
        ((HookFunction) thisObj).clearHooks();
    }

    @JSFunction
    public static Class<?> findClass(Context cx, Scriptable thisObj, Object[] args, Function funObj) throws ClassNotFoundException {
        return ((HookFunction) thisObj).findClass(args);
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
    public static ClassLoader getClassLoader(Context cx, Scriptable thisObj, Object[] args, Function funObj) {
        return ((HookFunction) thisObj).getClassLoader();
    }

    @JSFunction
    public static void help(Context cx, Scriptable thisObj, Object[] args, Function funObj) {
        JsConsole.fromScope(thisObj.getParentScope()).log(HELP);
    }

    @JSFunction
    public static ClassLoader[] getClassLoaders(Context cx, Scriptable thisObj, Object[] args, Function funObj) {
        return NativeUtils.getClassLoaders();
    }

    @JSFunction
    public static ClassLoader[] getClassLoaders2(Context cx, Scriptable thisObj, Object[] args, Function funObj) {
        return NativeUtils.getClassLoaders2();
    }

    @JSFunction
    public static Object trace(Context cx, Scriptable thisObj, Object[] args, Function funObj) {
        return ((HookFunction) thisObj).traceInternal(cx, thisObj.getParentScope(), args, funObj, false);
    }

    @JSFunction
    public static Object traces(Context cx, Scriptable thisObj, Object[] args, Function funObj) {
        return ((HookFunction) thisObj).traceInternal(cx, thisObj.getParentScope(), args, funObj, true);
    }

    private UnhookFunction traceInternal(Context cx, Scriptable scope, Object[] args, Function funObj, boolean stack) {
        var realArgs = new Object[args.length + 1];
        System.arraycopy(args, 0, realArgs, 0, args.length);
        var console = JsConsole.fromScope(scope);
        var counter = new AtomicInteger(0);
        realArgs[realArgs.length - 1] = new XC_MethodHook() {
            @Override
            protected void afterHookedMethod(MethodHookParam param) throws Throwable {
                var count = counter.incrementAndGet();
                try {
                    console.info(count, "method", param.method);
                    console.info(count, "thread", Thread.currentThread().toString(), Thread.currentThread());
                    if (!Modifier.isStatic(param.method.getModifiers())) {
                        console.info(count, "this", param.thisObject);
                    }
                    for (var i = 0; i < param.args.length; i++) {
                        console.info(count, "arg", i, param.args[i]);
                    }
                    if (param.hasThrowable()) {
                        console.info(count, "throwable", param.getThrowable());
                    } else {
                        if (param.method instanceof Method && ((Method) param.method).getReturnType() != Void.TYPE) {
                            console.info(count, "result", param.getResult());
                        }
                    }
                    if (stack) {
                        console.info(Utils.getStackTrace(false));
                    }
                } catch (Throwable t) {
                    console.error("error occurred while tracing", t);
                }
            }
        };
        var unhook = hook(cx, scope, realArgs);
        console.info("start tracing " + unhook.mUnhooks.size() + " methods (stackTrace=" + stack + ")");
        return unhook;
    }

    @JSFunction
    public static void deoptimizeMethod(Context cx, Scriptable thisObj, Object[] args, Function funObj) throws Throwable {
        XposedBridge.class.getDeclaredMethod("deoptimizeMethod", Member.class).invoke(null, args[0]);
    }

    private Object[] getObjectsOfClass(Object[] args) throws Throwable {
        if (args.length == 0) throw new IllegalArgumentException("usage: <clazz> [containsSubClass]");
        Class<?> target;
        var arg0 = args[0];
        if (arg0 instanceof Wrapper) arg0 = ((Wrapper) arg0).unwrap();
        if (arg0 instanceof Class) {
            target = (Class<?>) arg0;
        } else if (args[0] instanceof String) {
            target = getClassLoader().loadClass((String) arg0);
        } else {
            throw new IllegalArgumentException("arg 0 must be a class!");
        }
        boolean containsSubClass = false;
        if (args.length >= 2) {
            var arg1 = args[1];
            if (arg1 instanceof Wrapper) arg1 = ((Wrapper) arg1).unwrap();
            if (arg1 instanceof Boolean) containsSubClass = (Boolean) arg1;
            else throw new IllegalArgumentException("arg1 must be a boolean!");
        }
        return NativeUtils.getObjects(target, containsSubClass);
    }

    @JSFunction
    public static Object[] getObjectsOfClass(Context cx, Scriptable thisObj, Object[] args, Function funObj) throws Throwable {
        return ((HookFunction) thisObj).getObjectsOfClass(args);
    }

    private Class<?>[] getAssignableClasses(Object[] args) throws Throwable {
        if (args.length == 0) throw new IllegalArgumentException("usage: <clazz> [classloader]");
        Class<?> target;
        ClassLoader loader = null;
        var arg0 = args[0];
        if (arg0 instanceof Wrapper) {
            arg0 = ((Wrapper) arg0).unwrap();
        }
        if (arg0 instanceof Class) {
            target = (Class<?>) arg0;
        } else if (arg0 instanceof String) {
            target = getClassLoader().loadClass((String) arg0);
        } else {
            throw new IllegalArgumentException("arg 0 must be a class!");
        }
        if (args.length >= 2) {
            var arg1 = args[1];
            if (arg1 instanceof Wrapper) arg1 = ((Wrapper) arg1).unwrap();
            if (arg1 instanceof ClassLoader) loader = (ClassLoader) arg1;
            else throw new IllegalArgumentException("arg1 must be a ClassLoader!");
        }
        var classes = NativeUtils.getAssignableClasses(target, loader);
        if (loader != null) {
            List<Class<?>> realClasses = new LinkedList<>();
            for (var c: classes) {
                if (c.getClassLoader() == loader) realClasses.add(c);
            }
            return realClasses.toArray(new Class<?>[0]);
        }
        return classes;
    }

    @JSFunction
    public static Class<?>[] getAssignableClasses(Context cx, Scriptable thisObj, Object[] args, Function funObj) throws Throwable {
        return ((HookFunction) thisObj).getAssignableClasses(args);
    }

    @JSFunction
    public static Object packagesCL(Context cx, Scriptable thisObj, Object[] args, Function funObj) throws Throwable {
        return StethoxAppInterceptor.classLoaderMap;
    }

    @JSFunction
    public static void usePackage(Context cx, Scriptable thisObj, Object[] args, Function funObj) throws Throwable {
        ((HookFunction) thisObj).usePackage(args[0].toString());
    }

    private void usePackage(String pkg) {
        mClassLoader = StethoxAppInterceptor.classLoaderMap.get(pkg);
    }

    @JSFunction
    public static Object currentPkg(Context cx, Scriptable thisObj, Object[] args, Function funObj) throws Throwable {
        return ((HookFunction) thisObj).currentPkg();
    }

    private String currentPkg() {
        if (mClassLoader == null) return null;
        for (var entry : StethoxAppInterceptor.classLoaderMap.entrySet()) {
            if (entry.getValue().equals(mClassLoader)) {
                return entry.getKey();
            }
        }
        return null;
    }
}
