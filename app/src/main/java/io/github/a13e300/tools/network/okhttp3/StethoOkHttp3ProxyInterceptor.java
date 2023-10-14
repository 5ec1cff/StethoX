package io.github.a13e300.tools.network.okhttp3;

import androidx.annotation.Nullable;

import com.facebook.stetho.inspector.network.DefaultResponseHandler;
import com.facebook.stetho.inspector.network.NetworkEventReporter;
import com.facebook.stetho.inspector.network.NetworkEventReporterImpl;
import com.facebook.stetho.inspector.network.RequestBodyHelper;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;

import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import io.github.a13e300.tools.Logger;
import io.github.a13e300.tools.deobfuscation.NameMap;



public class StethoOkHttp3ProxyInterceptor {
    /*public static XC_MethodHook.Unhook startHookClientBuild(ClassLoader classLoader) {
        OkHttp3Helper helper = new DefaultOkHttp3Helper(classLoader, NameMap.sDefaultMap);
        var interceptor = new StethoOkHttp3ProxyInterceptor(helper);
        var proxy = Proxy.newProxyInstance(classLoader, new Class[]{helper.Interceptor()},
                (o, method, objects) -> {
                    if ("intercept".equals(method.getName()))
                        return interceptor.intercept(objects[0]);
                    try {
                        return method.invoke(o, objects);
                    } catch (InvocationTargetException e) {
                        if (e.getCause() != null) throw e.getCause();
                        throw new RuntimeException(e);
                    }
                }
        );
        return XposedBridge.hookMethod(helper.OkHttpClient$Builder_getNetworkInterceptors$okhttp(), new XC_MethodHook() {
            @Override
            protected void afterHookedMethod(MethodHookParam param) throws Throwable {
                var list = (List) param.getResult();
                if (list == null) list = new ArrayList();
                if (list.contains(proxy)) return;
                list.add(proxy);
                param.setResult(list);
            }
        });
        return null;
    }*/
    public static Set<XC_MethodHook.Unhook> start(ClassLoader classLoader) {
        try {
            return start(classLoader, false);
        } catch (Throwable t) {
            Logger.e("try deobfuscation for okhttp");
            return start(classLoader, true);
        }
    }

    public static Set<XC_MethodHook.Unhook> start(ClassLoader classLoader, boolean useDeObf) {
        if (useDeObf) {
            return start(classLoader, DeobfuscationUtilsKt.deobfOkhttp3(classLoader));
        } else {
            return start(classLoader, NameMap.sDefaultMap);
        }
    }

    public static Set<XC_MethodHook.Unhook> start(ClassLoader classLoader, NameMap nameMap) {
        OkHttp3Helper helper = new DefaultOkHttp3Helper(classLoader, nameMap);
        var interceptor = new StethoOkHttp3ProxyInterceptor(helper);
        var proxy = Proxy.newProxyInstance(classLoader, new Class[]{helper.Interceptor()},
                (o, method, objects) -> {
                    if ("intercept".equals(method.getName())) {
                        return interceptor.intercept(objects[0]);
                    } else if ("equals".equals(method.getName())) {
                        return o == objects[0];
                    } else if ("hashCode".equals(method.getName())) {
                        return System.identityHashCode(o);
                    } else if ("toString".equals(method.getName())) {
                        return o.getClass().getName() + "{" + System.identityHashCode(o) + "}";
                    } else {
                        throw new IllegalArgumentException("unknown method " + method + " to proxy!");
                    }
                }
        );
        return XposedBridge.hookAllConstructors(helper.okhttp3_internal_http_RealInterceptorChain(), new XC_MethodHook() {
            @Override
            protected void afterHookedMethod(MethodHookParam param) throws Throwable {
                // TODO: avoid using proxy
                var oldList = (List) helper.okhttp3_internal_http_RealInterceptorChain_interceptors().get(param.thisObject);
                if (oldList != null) {
                    if (oldList.contains(proxy)) return;
                    var list = new ArrayList();
                    list.addAll(oldList);
                    list.add(list.size() - 1, proxy);
                    Logger.d("add " + proxy + " to " + list);
                    helper.okhttp3_internal_http_RealInterceptorChain_interceptors().set(param.thisObject, list);
                    var updatedList = helper.okhttp3_internal_http_RealInterceptorChain_interceptors().get(param.thisObject);
                    Logger.d("updated=" + updatedList);
                }
            }
        });
    }

    private final NetworkEventReporter mEventReporter = NetworkEventReporterImpl.get();

    private final OkHttp3Helper mHelper;

    public StethoOkHttp3ProxyInterceptor(OkHttp3Helper helper) {
        mHelper = helper;
    }

    private Object /*Response*/ intercept(Object /*Chain*/ chain) throws IOException {
        Logger.d("intercept " + chain);
        String requestId = mEventReporter.nextRequestId();

        Object /*Request*/ request = mHelper.Interceptor$Chain_request(chain);

        RequestBodyHelper requestBodyHelper = null;
        if (mEventReporter.isEnabled()) {
            requestBodyHelper = new RequestBodyHelper(mEventReporter, requestId);
            OkHttpInspectorRequest inspectorRequest =
                    new OkHttpInspectorRequest(mHelper, requestId, request, requestBodyHelper);
            mEventReporter.requestWillBeSent(inspectorRequest);
        }

        Object /*Response*/ response;
        try {
            response = mHelper.Interceptor$Chain_proceed(chain, request);
        } catch (IOException e) {
            if (mEventReporter.isEnabled()) {
                mEventReporter.httpExchangeFailed(requestId, e.toString());
            }
            throw e;
        }

        if (mEventReporter.isEnabled()) {
            if (requestBodyHelper != null && requestBodyHelper.hasBody()) {
                requestBodyHelper.reportDataSent();
            }

            Object /*Connection*/ connection = mHelper.Interceptor$Chain_connection(chain);
            if (connection == null) {
                throw new IllegalStateException(
                        "No connection associated with this request; " +
                                "did you use addInterceptor instead of addNetworkInterceptor?");
            }
            mEventReporter.responseHeadersReceived(
                    new OkHttpInspectorResponse(
                            mHelper,
                            requestId,
                            request,
                            response,
                            connection));

            Object /*ResponseBody*/ body = mHelper.Response_body(response);
            Object /*MediaType*/ contentType = null;
            InputStream responseStream = null;
            if (body != null) {
                contentType = mHelper.ResponseBody_contentType(body);
                responseStream = mHelper.ResponseBody_byteStream(body);
            }

            responseStream = mEventReporter.interpretResponseStream(
                    requestId,
                    contentType != null ? contentType.toString() : null,
                    mHelper.Response_header(response, "Content-Encoding"),
                    responseStream,
                    new DefaultResponseHandler(mEventReporter, requestId));
            if (responseStream != null) {
                var responseBody = mHelper.ResponseBody_create(
                        mHelper.ResponseBody_contentType(body),
                        mHelper.ResponseBody_contentLength(body),
                        mHelper.Okio_1buffer(mHelper.Okio_source(responseStream))
                );
                response = mHelper.Response$Builder_build(
                        mHelper.Response$Builder_body(
                                mHelper.Response_newBuilder(response), responseBody
                        )
                );
            }
        }

        return response;
    }

    private static class OkHttpInspectorRequest implements NetworkEventReporter.InspectorRequest {
        private final String mRequestId;
        private final Object /*Request*/ mRequest;
        private RequestBodyHelper mRequestBodyHelper;
        private final OkHttp3Helper mHelper;

        public OkHttpInspectorRequest(
                OkHttp3Helper helper,
                String requestId,
                Object /*Request*/ request,
                RequestBodyHelper requestBodyHelper) {
            mHelper = helper;
            mRequestId = requestId;
            mRequest = request;
            mRequestBodyHelper = requestBodyHelper;
        }

        @Override
        public String id() {
            return mRequestId;
        }

        @Override
        public String friendlyName() {
            // Hmm, can we do better?  tag() perhaps?
            return null;
        }

        @Nullable
        @Override
        public Integer friendlyNameExtra() {
            return null;
        }

        @Override
        public String url() {
            return mHelper.Request_url(mRequest).toString();
        }

        @Override
        public String method() {
            return mHelper.Request_method(mRequest);
        }

        @Nullable
        @Override
        public byte[] body() throws IOException {
            Object /*RequestBody*/ body = mHelper.Request_body(mRequest);
            if (body == null) {
                return null;
            }
            OutputStream out = mRequestBodyHelper.createBodySink(firstHeaderValue("Content-Encoding"));
            try (var bufferedSink = mHelper.Okio_buffer(mHelper.Okio_sink(out))) {
                mHelper.RequestBody_writeTo(body, bufferedSink);
            }
            return mRequestBodyHelper.getDisplayBody();
        }

        @Override
        public int headerCount() {
            return mHelper.Headers_size(mHelper.Request_headers(mRequest));
        }

        @Override
        public String headerName(int index) {
            return mHelper.Headers_name(mHelper.Request_headers(mRequest), index);
        }

        @Override
        public String headerValue(int index) {
            return mHelper.Headers_value(mHelper.Request_headers(mRequest), index);
        }

        @Nullable
        @Override
        public String firstHeaderValue(String name) {
            return mHelper.Request_header(mRequest, name);
        }
    }

    private static class OkHttpInspectorResponse implements NetworkEventReporter.InspectorResponse {
        private final String mRequestId;
        private final Object /*Request*/ mRequest;
        private final Object /*Response*/ mResponse;
        private @Nullable
        final Object /*Connection*/ mConnection;
        private final OkHttp3Helper mHelper;

        public OkHttpInspectorResponse(
                OkHttp3Helper helper,
                String requestId,
                Object /*Request*/ request,
                Object /*Response*/ response,
                @Nullable Object /*Connection*/ connection) {
            mHelper = helper;
            mRequestId = requestId;
            mRequest = request;
            mResponse = response;
            mConnection = connection;
        }

        @Override
        public String requestId() {
            return mRequestId;
        }

        @Override
        public String url() {
            return mHelper.Request_url(mRequest).toString();
        }

        @Override
        public int statusCode() {
            return mHelper.Response_code(mResponse);
        }

        @Override
        public String reasonPhrase() {
            return mHelper.Response_message(mResponse);
        }

        @Override
        public boolean connectionReused() {
            // Not sure...
            return false;
        }

        @Override
        public int connectionId() {
            return mConnection == null ? 0 : mConnection.hashCode();
        }

        @Override
        public boolean fromDiskCache() {
            return mHelper.Response_cacheResponse(mResponse) != null;
        }

        @Override
        public int headerCount() {
            return mHelper.Headers_size(mHelper.Response_headers(mResponse));
        }

        @Override
        public String headerName(int index) {
            return mHelper.Headers_name(mHelper.Response_headers(mResponse), index);
        }

        @Override
        public String headerValue(int index) {
            return mHelper.Headers_value(mHelper.Response_headers(mResponse), index);
        }

        @Nullable
        @Override
        public String firstHeaderValue(String name) {
            return mHelper.Response_header(mResponse, name);
        }
    }
}
