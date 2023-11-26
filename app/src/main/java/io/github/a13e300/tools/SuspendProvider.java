package io.github.a13e300.tools;

import android.annotation.SuppressLint;
import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import java.util.Objects;

public class SuspendProvider extends ContentProvider {

    private SharedPreferences mSp;
    public SuspendProvider() {
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException("Not yet implemented");
    }

    @Override
    public String getType(Uri uri) {
        throw new UnsupportedOperationException("Not yet implemented");
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException("Not yet implemented");
    }

    @Override
    public boolean onCreate() {
        var ctx = getContext();
        if (ctx == null) return false;
        mSp = ctx.createDeviceProtectedStorageContext().getSharedPreferences("suspend", Context.MODE_PRIVATE);
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
                        String[] selectionArgs, String sortOrder) {
        throw new UnsupportedOperationException("Not yet implemented");
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection,
                      String[] selectionArgs) {
        throw new UnsupportedOperationException("Not yet implemented");
    }

    private void checkPermission() {
        Objects.requireNonNull(getContext()).enforceCallingPermission("android.permission.INTERACT_ACROSS_USERS", "permission denied");
    }

    @SuppressLint("ApplySharedPref")
    @Override
    public Bundle call(String method, String arg, Bundle extras) {
        Log.d("suspend", "called method " + method);
        var result = new Bundle();
        if ("process_started".equals(method)) {
            if (arg == null) {
                throw new IllegalArgumentException("process name is none");
            }
            var mode = mSp.getString(arg, null);
            boolean suspend = false;
            if ("oneshot".equals(mode)) {
                mSp.edit().remove(arg).commit();
                suspend = true;
            } else if ("always".equals(mode)) {
                suspend = true;
            }
            result.putBoolean("suspend", suspend);
        } else if ("suspend".equals(method)) {
            checkPermission();
            if (arg == null) {
                throw new IllegalArgumentException("process name is none");
            }
            mSp.edit().putString(arg, "oneshot").commit();
        } else if ("suspend_forever".equals(method)) {
            checkPermission();
            if (arg == null) {
                throw new IllegalArgumentException("process name is none");
            }
            mSp.edit().putString(arg, "always").commit();
        } else if ("clear".equals(method)) {
            checkPermission();
            if (arg == null) {
                mSp.edit().clear().commit();
            } else {
                mSp.edit().remove(arg).commit();
            }
        } else if ("list".equals(method)) {
            checkPermission();
            mSp.getAll().forEach((k, v) -> { result.putString(k, (String) v); });
        }
        return result;
    }
}