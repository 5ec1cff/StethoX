package io.github.a13e300.tools.objects;

import org.mozilla.javascript.BaseFunction;
import org.mozilla.javascript.Context;
import org.mozilla.javascript.Scriptable;
import org.mozilla.javascript.ScriptableObject;
import org.mozilla.javascript.Wrapper;

import java.lang.reflect.Array;

public class JArrayFunction extends BaseFunction {
    private final Class<?> mType;

    public JArrayFunction(Class<?> type) {
        mType = type;
    }

    @Override
    public Object call(Context cx, Scriptable scope, Scriptable thisObj, Object[] args) {
        Class<?> type = null;
        int length = -1;
        if (mType != null) {
            type = mType;
            if (args.length != 1 || !(args[0] instanceof Number)) throw new IllegalArgumentException("required 1 numeric arg: length");
            length = ((Number) args[0]).intValue();
        } else {
            if (args.length == 2) {
                Object a0 = args[0];
                if (a0 instanceof Wrapper) a0 = ((Wrapper) a0).unwrap();
                if (a0 instanceof String) {
                    Object hook = ScriptableObject.getProperty(scope, "hook");
                    if (hook instanceof HookFunction) {
                        try {
                            type = ((HookFunction) hook).getClassLoader().loadClass((String) a0);
                        } catch (ClassNotFoundException e) {
                            throw new IllegalArgumentException(e);
                        }
                    } else {
                        try {
                            type = Class.forName((String) a0);
                        } catch (ClassNotFoundException e) {
                            throw new IllegalArgumentException(e);
                        }
                    }
                } else if (a0 instanceof Class) {
                    type = (Class<?>) a0;
                }
                if (args[1] instanceof Number) length = ((Number) args[1]).intValue();
            }
            if (type == null || length == -1) throw new IllegalArgumentException("2 args required: class (String|Class), length");
        }
        return Array.newInstance(type, length);
    }
}
