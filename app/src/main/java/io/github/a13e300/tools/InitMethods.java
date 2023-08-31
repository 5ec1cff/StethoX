package io.github.a13e300.tools;

import java.util.List;

public class InitMethods {
    public static final String INIT_ATTACH = "attach";
    public static final String INIT_ATTACH_BASE_CONTEXT = "attach_base_context";
    public static final String INIT_MAKE_APPLICATION = "make_application";
    public static final String INIT_DEFAULT = INIT_MAKE_APPLICATION;

    public static List<String> METHODS = List.of(INIT_ATTACH, INIT_ATTACH_BASE_CONTEXT, INIT_MAKE_APPLICATION);
}
