package io.github.a13e300.tools.deobfuscation;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class MutableNameMap implements NameMap {
    private static class ClassNode {
        String name;
        final Map<String, Map<List<String>, String>> methodMap = new HashMap<>();
        final Map<String, String> fieldMap = new HashMap<>();
    }
    private final Map<String, ClassNode> mClassMap = new HashMap<>();
    @Override
    public String getClass(String name) {
        var node = mClassMap.get(name);
        if (node == null) return name;
        return node.name;
    }

    public void putClass(String name, String obfName) {
        mClassMap.computeIfAbsent(name, k -> new ClassNode()).name = obfName;
    }

    @Override
    public String getMethod(String className, String methodName, String... argTypes) {
        var node = mClassMap.get(className);
        if (node == null) return methodName;
        var mm = node.methodMap.get(methodName);
        if (mm == null) return methodName;
        var n = mm.get(Arrays.asList(argTypes));
        if (n == null) return methodName;
        return n;
    }

    public void putMethod(String className, String methodName, String obfName, String... argTypes) {
        mClassMap.computeIfAbsent(className, k -> new ClassNode())
                .methodMap.computeIfAbsent(methodName, k -> new HashMap<>())
                .put(Arrays.asList(argTypes), obfName);
    }

    @Override
    public String getField(String className, String fieldName) {
        var node = mClassMap.get(className);
        if (node == null) return fieldName;
        var n = node.fieldMap.get(fieldName);
        if (n == null) return fieldName;
        return n;
    }

    public void putField(String className, String fieldName, String obfName) {
        mClassMap.computeIfAbsent(className, k -> new ClassNode())
                .fieldMap.put(fieldName, obfName);
    }
}
