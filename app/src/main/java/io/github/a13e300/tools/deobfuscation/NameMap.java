package io.github.a13e300.tools.deobfuscation;

public interface NameMap {
    NameMap sDefaultMap = new NameMap() {
        @Override
        public String getClass(String name) {
            return name;
        }

        @Override
        public String getMethod(String className, String methodName, String... argTypes) {
            return methodName;
        }

        @Override
        public String getField(String className, String fieldName) {
            return fieldName;
        }
    };
    String getClass(String name);
    String getMethod(String className, String methodName, String... argTypes);
    String getField(String className, String fieldName);
}
