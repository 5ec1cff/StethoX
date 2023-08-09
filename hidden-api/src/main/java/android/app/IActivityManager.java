package android.app;

import android.os.IBinder;

public interface IActivityManager {
    void showWaitingForDebugger(IApplicationThread who, boolean waiting);

    class Stub {
        public static IActivityManager asInterface(IBinder b) {
            throw new RuntimeException("");
        }
    }
}
