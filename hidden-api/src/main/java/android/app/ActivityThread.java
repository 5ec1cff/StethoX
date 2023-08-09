package android.app;

public class ActivityThread {
    public String getProcessName() {
        throw new RuntimeException("STUB");
    }

    public ApplicationThread getApplicationThread() {
        throw new RuntimeException("STUB");
    }

    public static ActivityThread currentActivityThread() {
        throw new RuntimeException("STUB");
    }

    private static class ApplicationThread implements IApplicationThread {}
}
