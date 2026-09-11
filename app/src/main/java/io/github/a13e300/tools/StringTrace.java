package io.github.a13e300.tools;

import android.util.Log;

import java.lang.reflect.Modifier;
import java.util.concurrent.ConcurrentHashMap;

public class StringTrace {
    public static int STRING_TRACE_FLAG_EQUALS = 1;
    public static int STRING_TRACE_FLAG_STARTSWITH = 1 << 1;
    public static int STRING_TRACE_FLAG_ENDSWITH = 1 << 2;
    public static int STRING_TRACE_FLAG_CONTAINS = 1 << 3;
    public static int STRING_TRACE_FLAG_REGEX = 1 << 4;
    public static int STRING_TRACE_FLAG_IGNORECASE = 1 << 5;

    static {
        try {
            Class.forName(NativeUtils.class.getName(), true, NativeUtils.class.getClassLoader());
        } catch (ClassNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    private static boolean initialized = false;

    private synchronized static void initStringTrace() {
        if (initialized) return;
        if (!setupStringTraceNative()) {
            throw new RuntimeException("Could not init StringTrace!");
        }
        try {
            var sf = Class.forName("java.lang.StringFactory");
            for (var m: sf.getDeclaredMethods()) {
                if ((m.getModifiers() & Modifier.NATIVE) == 0) {
                    Utils.deoptimizeMethod(m);
                }
            }
        } catch (Throwable t) {
            Log.e("StethoX", "deoptimize StringFactory", t);
        }
        initialized = true;
    }

    static class StringTraceRecord {
        int count;
        String matchedString;
        String stackTrace;
    }

    static class StringTraceSession {
        int flags;
        int id;
        int reportFromCount;
        int totalCount;
        StringTraceRecord[] records;
        boolean asyncTrace;
        boolean end;
    }

    static ConcurrentHashMap<Integer, StringTraceSession> sessions = new ConcurrentHashMap<>();

    private static native boolean setupStringTraceNative();

    private static native int registerStringTraceNative(String keyword, int flag, int reportFromCount, int totalCount);
    private static native void unregisterStringTraceNative(int id);

    public static native void markStringTraceIgnore(boolean ignore);

    private static void reportStringTrace(String s, int[] idsAndHitCounts) {
        try {
            String traceStr = null, asyncTraceStr = null;
            for (int i = 0; i < idsAndHitCounts.length / 2; i++) {
                var id = idsAndHitCounts[2 * i];
                var hit = idsAndHitCounts[2 * i + 1];
                var session = sessions.get(id);
                if (session != null) {
                    var record = new StringTraceRecord();
                    if (traceStr == null) {
                        traceStr = "(thread " + Thread.currentThread().getId() + ")\n" + Utils.getStackTrace(false);
                    }
                    record.count = hit;
                    record.matchedString = s;
                    if (session.asyncTrace) {
                        if (asyncTraceStr == null) {
                            asyncTraceStr = traceStr + AsyncTraceKt.asyncStackTrace();
                        }
                        record.stackTrace = asyncTraceStr;
                    } else {
                        record.stackTrace = traceStr;
                    }
                    synchronized (session) {
                        var n = hit - session.reportFromCount;
                        if (n >= 0 && n < session.records.length) {
                            session.records[n] = record;
                        }
                        if (hit == session.totalCount && !session.end) {
                            session.end = true;
                            if (session.asyncTrace)
                                AsyncTraceKt.exitAsyncTrace();
                        }
                    }
                }
            }
        } catch (Throwable t) {
            Log.e("StethoX", "reportStringTrace", t);
        }
    }

    public static int registerStringTrace(String keyword, String options, int reportFromCount, int totalCount) {
        initStringTrace();
        if (keyword == null) throw new IllegalArgumentException("keyword should not be null");
        if (options == null) throw new IllegalArgumentException("options: eq/equals, sw/startswith, ew/endswith, c/contains, ic/ignorecase, at/asynctrace");
        if (reportFromCount < 0 || reportFromCount >= totalCount || totalCount < 0)
            throw new IllegalArgumentException("0 <= reportFromCount < totalCount");
        int flags = 0;
        boolean asyncTrace = false;
        for (var opt: options.split(",")) {
            switch (opt) {
                case "eq", "equals" -> {
                    if ((flags & (STRING_TRACE_FLAG_STARTSWITH | STRING_TRACE_FLAG_ENDSWITH | STRING_TRACE_FLAG_CONTAINS)) != 0) {
                        throw new IllegalArgumentException("conflict options");
                    }
                    flags |= STRING_TRACE_FLAG_EQUALS;
                }
                case "sw", "startswith" -> {
                    if ((flags & (STRING_TRACE_FLAG_EQUALS | STRING_TRACE_FLAG_ENDSWITH | STRING_TRACE_FLAG_CONTAINS)) != 0) {
                        throw new IllegalArgumentException("conflict options");
                    }
                    flags |= STRING_TRACE_FLAG_STARTSWITH;
                }
                case "ew", "endswith" -> {
                    if ((flags & (STRING_TRACE_FLAG_EQUALS | STRING_TRACE_FLAG_STARTSWITH | STRING_TRACE_FLAG_CONTAINS)) != 0) {
                        throw new IllegalArgumentException("conflict options");
                    }
                    flags |= STRING_TRACE_FLAG_ENDSWITH;
                }
                case "c", "contains" -> {
                    if ((flags & (STRING_TRACE_FLAG_EQUALS | STRING_TRACE_FLAG_STARTSWITH | STRING_TRACE_FLAG_ENDSWITH)) != 0) {
                        throw new IllegalArgumentException("conflict options");
                    }
                    flags |= STRING_TRACE_FLAG_CONTAINS;
                }
                case "ic", "ignorecase" -> {
                    flags |= STRING_TRACE_FLAG_IGNORECASE;
                }
                case "at", "asynctrace" -> {
                    asyncTrace = true;
                }
                default -> {
                    throw new IllegalArgumentException("options: eq/equals, sw/startswith, ew/endswith, c/contains, ic/ignorecase, at/asynctrace");
                }
            }
        }
        var res = registerStringTraceNative(keyword, flags, reportFromCount, totalCount);
        if (res < 0) {
            throw new RuntimeException("registerStringTraceNative failed");
        }
        var session = new StringTraceSession();
        session.asyncTrace = asyncTrace;
        session.id = res;
        session.records = new StringTraceRecord[totalCount - reportFromCount];
        session.reportFromCount = reportFromCount;
        session.totalCount = totalCount;
        session.flags = flags;
        if (asyncTrace) {
            AsyncTraceKt.enterAsyncTrace();;
        }
        sessions.put(res, session);
        return res;
    }

    public static void unregisterStringTrace(int id) {
        initStringTrace();
        var s = sessions.remove(id);
        if (s != null) {
            synchronized (s) {
                if (!s.end && s.asyncTrace) {
                    AsyncTraceKt.exitAsyncTrace();
                    s.end = true;
                }
            }
        }
        unregisterStringTraceNative(id);
    }

    public static void clearSessions() {
        initStringTrace();
        for (var s: sessions.keySet()) {
            unregisterStringTrace(s);
        }
        sessions.clear();
    }
}
