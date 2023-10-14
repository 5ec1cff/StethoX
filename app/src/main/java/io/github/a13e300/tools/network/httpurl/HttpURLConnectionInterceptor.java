package io.github.a13e300.tools.network.httpurl;

import com.facebook.stetho.urlconnection.StethoURLConnectionManager;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.ProtocolException;
import java.net.URL;
import java.security.Permission;

import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedHelpers;

public class HttpURLConnectionInterceptor {
    private static XC_MethodHook.Unhook sUnhook;
    public static synchronized void enable() {
        sUnhook = XposedHelpers.findAndHookMethod(URL.class, "openConnection", new XC_MethodHook() {
            @Override
            protected void afterHookedMethod(MethodHookParam param) throws Throwable {
                var connection = param.getResult();
                if (!(connection instanceof HttpURLConnection)) return;

            }
        });
    }

    public static synchronized void disable() {
        if (sUnhook != null) {
            sUnhook.unhook();
            sUnhook = null;
        }
    }

    private class HttpURLConnectionWrapper extends HttpURLConnection {
        private final HttpURLConnection mWrapped;
        private final StethoURLConnectionManager mManager;
        private ByteArrayOutputStream mOutputStream;
        HttpURLConnectionWrapper(HttpURLConnection wrapped) {
            super(wrapped.getURL());
            mWrapped = wrapped;
            mManager = new StethoURLConnectionManager(null);
        }

        @Override
        public void connect() throws IOException {

        }

        @Override
        public boolean getDoOutput() {
            return mWrapped.getDoOutput();
        }

        @Override
        public void setDoOutput(boolean dooutput) {
            mWrapped.setDoOutput(dooutput);
        }

        @Override
        public OutputStream getOutputStream() throws IOException {
            // TODO
            return mWrapped.getOutputStream();
        }

        public String getHeaderFieldKey(int n) {
            return mWrapped.getHeaderFieldKey(n);
        }

        public void setFixedLengthStreamingMode(int contentLength) {
            mWrapped.setFixedLengthStreamingMode(contentLength);
        }

        public void setFixedLengthStreamingMode(long contentLength) {
            mWrapped.setFixedLengthStreamingMode(contentLength);
        }

        public void setChunkedStreamingMode(int chunklen) {
            mWrapped.setChunkedStreamingMode(chunklen);
        }

        public String getHeaderField(int n) {
            return mWrapped.getHeaderField(n);
        }

        public void setInstanceFollowRedirects(boolean followRedirects) {
            mWrapped.setInstanceFollowRedirects(followRedirects);
        }

        public boolean getInstanceFollowRedirects() {
            return mWrapped.getInstanceFollowRedirects();
        }

        public void setRequestMethod(String method) throws ProtocolException {
            mWrapped.setRequestMethod(method);
        }

        public String getRequestMethod() {
            return mWrapped.getRequestMethod();
        }

        public int getResponseCode() throws IOException {
            return mWrapped.getResponseCode();
        }

        public String getResponseMessage() throws IOException {
            return mWrapped.getResponseMessage();
        }

        public long getHeaderFieldDate(String name, long Default) {
            return mWrapped.getHeaderFieldDate(name, Default);
        }

        public void disconnect() {
            mWrapped.disconnect();
        }

        public boolean usingProxy() {
            return mWrapped.usingProxy();
        }

        public Permission getPermission() throws IOException {
            return mWrapped.getPermission();
        }

        public InputStream getErrorStream() {
            return mWrapped.getErrorStream();
        }
    }
}
