package io.github.a13e300.tools;

import android.app.Activity;
import android.os.Bundle;

import androidx.annotation.Nullable;

public class HiddenApiActivity extends Activity {
    static {
        try {
            System.loadLibrary("stethox");
        } catch (Throwable t) {
            Logger.e("load", t);
        }
    }

    private static native void testHiddenApi();

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        testHiddenApi();
    }
}
