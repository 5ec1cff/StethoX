package io.github.a13e300.tools;

import androidx.annotation.NonNull;

import org.luckypray.dexkit.DexKitBridge;

import java.util.Objects;

public class DexKitWrapper {

    static {
        try {
            System.loadLibrary("dexkit");
        } catch (Throwable t) {
            Logger.e("failed to load dexkit", t);
        }
    }

    @NonNull
    public static DexKitBridge create(String apkPath) {
        return Objects.requireNonNull(DexKitBridge.create(apkPath), "dexkit failed to create");
    }

    @NonNull
    public static DexKitBridge create(ClassLoader cl) {
        return Objects.requireNonNull(DexKitBridge.create(cl, true), "dexkit failed to create");
    }

    @NonNull
    public static DexKitBridge create(ClassLoader cl, boolean useMemory) {
        return Objects.requireNonNull(DexKitBridge.create(cl, useMemory), "dexkit failed to create");
    }

    @NonNull
    public static DexKitBridge create(byte[][] dex) {
        return Objects.requireNonNull(DexKitBridge.create(dex), "dexkit failed to create");
    }
}
