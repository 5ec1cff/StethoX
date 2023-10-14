package io.github.a13e300.tools.network.okhttp3;

import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import io.github.a13e300.tools.deobfuscation.NameMap;

public class DefaultOkHttp3Helper implements OkHttp3Helper {
    private Method method_Interceptor$Chain_request;

    @Override
    public Object /*Request*/ Interceptor$Chain_request(Object chain) {
        try {
            return (Object) method_Interceptor$Chain_request.invoke(chain);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Interceptor$Chain_proceed;

    @Override
    public Object /*Response*/ Interceptor$Chain_proceed(Object chain, Object /*Request*/ request) throws IOException {
        try {
            return (Object) method_Interceptor$Chain_proceed.invoke(chain, request);
        } catch (InvocationTargetException ex) {
            if (ex.getCause() instanceof IOException) {
                throw (IOException) ex.getCause();
            } else throw new RuntimeException(ex);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Interceptor$Chain_connection;

    @Override
    public Object /*Connection*/ Interceptor$Chain_connection(Object chain) {
        try {
            return (Object) method_Interceptor$Chain_connection.invoke(chain);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Response_body;

    @Override
    public Object /*ResponseBody*/ Response_body(Object response) {
        try {
            return (Object) method_Response_body.invoke(response);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Response_header;

    @Override
    public String Response_header(Object response, String name) {
        try {
            return (String) method_Response_header.invoke(response, name);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Response_headers;

    @Override
    public Object /*Headers*/ Response_headers(Object response) {
        try {
            return (Object) method_Response_headers.invoke(response);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Response_cacheResponse;

    @Override
    public Object /*Response*/ Response_cacheResponse(Object response) {
        try {
            return (Object) method_Response_cacheResponse.invoke(response);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Response_newBuilder;

    @Override
    public Object /*Response$Builder*/ Response_newBuilder(Object response) {
        try {
            return (Object) method_Response_newBuilder.invoke(response);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Response$Builder_build;

    @Override
    public Object /*Response*/ Response$Builder_build(Object builder) {
        try {
            return (Object) method_Response$Builder_build.invoke(builder);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_ResponseBody_create;

    @Override
    public     /*static*/ Object /*ResponseBody*/ ResponseBody_create(Object /*MediaType*/ contentType, long contentLength, Object /*BufferedSource*/ content) {
        try {
            return (Object) method_ResponseBody_create.invoke(null, contentType, contentLength, content);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Response$Builder_body;

    @Override
    public Object /*Response$Builder*/ Response$Builder_body(Object builder, Object /*ResponseBody*/ body) {
        try {
            return (Object) method_Response$Builder_body.invoke(builder, body);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_ResponseBody_contentType;

    @Override
    public Object /*MediaType*/ ResponseBody_contentType(Object body) {
        try {
            return (Object) method_ResponseBody_contentType.invoke(body);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_ResponseBody_byteStream;

    @Override
    public InputStream ResponseBody_byteStream(Object body) {
        try {
            return (InputStream) method_ResponseBody_byteStream.invoke(body);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_ResponseBody_contentLength;

    @Override
    public long ResponseBody_contentLength(Object body) {
        try {
            return (long) method_ResponseBody_contentLength.invoke(body);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Request_url;

    @Override
    public Object /*HttpUrl*/ Request_url(Object request) {
        try {
            return (Object) method_Request_url.invoke(request);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Request_method;

    @Override
    public String Request_method(Object request) {
        try {
            return (String) method_Request_method.invoke(request);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Request_body;

    @Override
    public Object /*RequestBody*/ Request_body(Object request) {
        try {
            return (Object) method_Request_body.invoke(request);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Request_headers;

    @Override
    public Object /*Headers*/ Request_headers(Object request) {
        try {
            return (Object) method_Request_headers.invoke(request);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Request_header;

    @Override
    public String Request_header(Object request, String name) {
        try {
            return (String) method_Request_header.invoke(request, name);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_RequestBody_writeTo;

    @Override
    public void RequestBody_writeTo(Object body, Closeable /*BufferedSink*/ bufferedSink) throws IOException {
        try {
            method_RequestBody_writeTo.invoke(body, bufferedSink);
        } catch (InvocationTargetException ex) {
            if (ex.getCause() instanceof IOException) {
                throw (IOException) ex.getCause();
            } else throw new RuntimeException(ex);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Headers_size;

    @Override
    public int Headers_size(Object headers) {
        try {
            return (int) method_Headers_size.invoke(headers);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Headers_name;

    @Override
    public String Headers_name(Object headers, int index) {
        try {
            return (String) method_Headers_name.invoke(headers, index);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Headers_value;

    @Override
    public String Headers_value(Object headers, int index) {
        try {
            return (String) method_Headers_value.invoke(headers, index);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Response_code;

    @Override
    public int Response_code(Object response) {
        try {
            return (int) method_Response_code.invoke(response);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Response_message;

    @Override
    public String Response_message(Object response) {
        try {
            return (String) method_Response_message.invoke(response);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Okio_sink;

    @Override
    public     /*static*/ Closeable /*Sink*/ Okio_sink(OutputStream out) {
        try {
            return (Closeable) method_Okio_sink.invoke(null, out);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Okio_buffer;

    @Override
    public     /*static*/ Closeable /*BufferedSink*/ Okio_buffer(Closeable /*Sink*/ sink) {
        try {
            return (Closeable) method_Okio_buffer.invoke(null, sink);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Okio_source;

    @Override
    public     /*static*/ Closeable /*Source*/ Okio_source(InputStream in) {
        try {
            return (Closeable) method_Okio_source.invoke(null, in);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Method method_Okio_1buffer;

    @Override
    public     /*static*/ Closeable /*BufferedSource*/ Okio_1buffer(Closeable /*Source*/ source) {
        try {
            return (Closeable) method_Okio_1buffer.invoke(null, source);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private Class class_Interceptor;

    @Override
    public Class Interceptor() {
        return class_Interceptor;
    }

    private Class class_okhttp3_internal_http_RealInterceptorChain;

    @Override
    public Class okhttp3_internal_http_RealInterceptorChain() {
        return class_okhttp3_internal_http_RealInterceptorChain;
    }

    private Field field_okhttp3_internal_http_RealInterceptorChain_interceptors;

    @Override
    public Field okhttp3_internal_http_RealInterceptorChain_interceptors() {
        return field_okhttp3_internal_http_RealInterceptorChain_interceptors;
    }

    public DefaultOkHttp3Helper(ClassLoader classLoader, NameMap nameMap) {
        try {
            method_Interceptor$Chain_request = classLoader.loadClass(nameMap.getClass("okhttp3.Interceptor$Chain")).getDeclaredMethod(nameMap.getMethod("okhttp3.Interceptor$Chain", "request"));
            method_Interceptor$Chain_request.setAccessible(true);
            method_Interceptor$Chain_proceed = classLoader.loadClass(nameMap.getClass("okhttp3.Interceptor$Chain")).getDeclaredMethod(nameMap.getMethod("okhttp3.Interceptor$Chain", "proceed", "okhttp3.Request"), classLoader.loadClass("okhttp3.Request"));
            method_Interceptor$Chain_proceed.setAccessible(true);
            method_Interceptor$Chain_connection = classLoader.loadClass(nameMap.getClass("okhttp3.Interceptor$Chain")).getDeclaredMethod(nameMap.getMethod("okhttp3.Interceptor$Chain", "connection"));
            method_Interceptor$Chain_connection.setAccessible(true);
            method_Response_body = classLoader.loadClass(nameMap.getClass("okhttp3.Response")).getDeclaredMethod(nameMap.getMethod("okhttp3.Response", "body"));
            method_Response_body.setAccessible(true);
            method_Response_header = classLoader.loadClass(nameMap.getClass("okhttp3.Response")).getDeclaredMethod(nameMap.getMethod("okhttp3.Response", "header", "java.lang.String"), String.class);
            method_Response_header.setAccessible(true);
            method_Response_headers = classLoader.loadClass(nameMap.getClass("okhttp3.Response")).getDeclaredMethod(nameMap.getMethod("okhttp3.Response", "headers"));
            method_Response_headers.setAccessible(true);
            method_Response_cacheResponse = classLoader.loadClass(nameMap.getClass("okhttp3.Response")).getDeclaredMethod(nameMap.getMethod("okhttp3.Response", "cacheResponse"));
            method_Response_cacheResponse.setAccessible(true);
            method_Response_newBuilder = classLoader.loadClass(nameMap.getClass("okhttp3.Response")).getDeclaredMethod(nameMap.getMethod("okhttp3.Response", "newBuilder"));
            method_Response_newBuilder.setAccessible(true);
            method_Response$Builder_build = classLoader.loadClass(nameMap.getClass("okhttp3.Response$Builder")).getDeclaredMethod(nameMap.getMethod("okhttp3.Response$Builder", "build"));
            method_Response$Builder_build.setAccessible(true);
            method_ResponseBody_create = classLoader.loadClass(nameMap.getClass("okhttp3.ResponseBody")).getDeclaredMethod(nameMap.getMethod("okhttp3.ResponseBody", "create", "okhttp3.MediaType", "long", "okio.BufferedSource"), classLoader.loadClass("okhttp3.MediaType"), long.class, classLoader.loadClass("okio.BufferedSource"));
            method_ResponseBody_create.setAccessible(true);
            method_Response$Builder_body = classLoader.loadClass(nameMap.getClass("okhttp3.Response$Builder")).getDeclaredMethod(nameMap.getMethod("okhttp3.Response$Builder", "body", "okhttp3.ResponseBody"), classLoader.loadClass("okhttp3.ResponseBody"));
            method_Response$Builder_body.setAccessible(true);
            method_ResponseBody_contentType = classLoader.loadClass(nameMap.getClass("okhttp3.ResponseBody")).getDeclaredMethod(nameMap.getMethod("okhttp3.ResponseBody", "contentType"));
            method_ResponseBody_contentType.setAccessible(true);
            method_ResponseBody_byteStream = classLoader.loadClass(nameMap.getClass("okhttp3.ResponseBody")).getDeclaredMethod(nameMap.getMethod("okhttp3.ResponseBody", "byteStream"));
            method_ResponseBody_byteStream.setAccessible(true);
            method_ResponseBody_contentLength = classLoader.loadClass(nameMap.getClass("okhttp3.ResponseBody")).getDeclaredMethod(nameMap.getMethod("okhttp3.ResponseBody", "contentLength"));
            method_ResponseBody_contentLength.setAccessible(true);
            method_Request_url = classLoader.loadClass(nameMap.getClass("okhttp3.Request")).getDeclaredMethod(nameMap.getMethod("okhttp3.Request", "url"));
            method_Request_url.setAccessible(true);
            method_Request_method = classLoader.loadClass(nameMap.getClass("okhttp3.Request")).getDeclaredMethod(nameMap.getMethod("okhttp3.Request", "method"));
            method_Request_method.setAccessible(true);
            method_Request_body = classLoader.loadClass(nameMap.getClass("okhttp3.Request")).getDeclaredMethod(nameMap.getMethod("okhttp3.Request", "body"));
            method_Request_body.setAccessible(true);
            method_Request_headers = classLoader.loadClass(nameMap.getClass("okhttp3.Request")).getDeclaredMethod(nameMap.getMethod("okhttp3.Request", "headers"));
            method_Request_headers.setAccessible(true);
            method_Request_header = classLoader.loadClass(nameMap.getClass("okhttp3.Request")).getDeclaredMethod(nameMap.getMethod("okhttp3.Request", "header", "java.lang.String"), String.class);
            method_Request_header.setAccessible(true);
            method_RequestBody_writeTo = classLoader.loadClass(nameMap.getClass("okhttp3.RequestBody")).getDeclaredMethod(nameMap.getMethod("okhttp3.RequestBody", "writeTo", "okio.BufferedSink"), classLoader.loadClass("okio.BufferedSink"));
            method_RequestBody_writeTo.setAccessible(true);
            method_Headers_size = classLoader.loadClass(nameMap.getClass("okhttp3.Headers")).getDeclaredMethod(nameMap.getMethod("okhttp3.Headers", "size"));
            method_Headers_size.setAccessible(true);
            method_Headers_name = classLoader.loadClass(nameMap.getClass("okhttp3.Headers")).getDeclaredMethod(nameMap.getMethod("okhttp3.Headers", "name", "int"), int.class);
            method_Headers_name.setAccessible(true);
            method_Headers_value = classLoader.loadClass(nameMap.getClass("okhttp3.Headers")).getDeclaredMethod(nameMap.getMethod("okhttp3.Headers", "value", "int"), int.class);
            method_Headers_value.setAccessible(true);
            method_Response_code = classLoader.loadClass(nameMap.getClass("okhttp3.Response")).getDeclaredMethod(nameMap.getMethod("okhttp3.Response", "code"));
            method_Response_code.setAccessible(true);
            method_Response_message = classLoader.loadClass(nameMap.getClass("okhttp3.Response")).getDeclaredMethod(nameMap.getMethod("okhttp3.Response", "message"));
            method_Response_message.setAccessible(true);
            method_Okio_sink = classLoader.loadClass(nameMap.getClass("okio.Okio")).getDeclaredMethod(nameMap.getMethod("okio.Okio", "sink", "OutputStream"), OutputStream.class);
            method_Okio_sink.setAccessible(true);
            method_Okio_buffer = classLoader.loadClass(nameMap.getClass("okio.Okio")).getDeclaredMethod(nameMap.getMethod("okio.Okio", "buffer", "okio.Sink"), classLoader.loadClass("okio.Sink"));
            method_Okio_buffer.setAccessible(true);
            method_Okio_source = classLoader.loadClass(nameMap.getClass("okio.Okio")).getDeclaredMethod(nameMap.getMethod("okio.Okio", "source", "InputStream"), InputStream.class);
            method_Okio_source.setAccessible(true);
            method_Okio_1buffer = classLoader.loadClass(nameMap.getClass("okio.Okio")).getDeclaredMethod(nameMap.getMethod("okio.Okio", "buffer", "okio.Source"), classLoader.loadClass("okio.Source"));
            method_Okio_1buffer.setAccessible(true);
            class_Interceptor = classLoader.loadClass(nameMap.getClass("okhttp3.Interceptor"));
            class_okhttp3_internal_http_RealInterceptorChain = classLoader.loadClass(nameMap.getClass("okhttp3.internal.http.RealInterceptorChain"));
            field_okhttp3_internal_http_RealInterceptorChain_interceptors = classLoader.loadClass(nameMap.getClass("okhttp3.internal.http.RealInterceptorChain")).getDeclaredField(nameMap.getField("okhttp3.internal.http.RealInterceptorChain", "interceptors"));
            field_okhttp3_internal_http_RealInterceptorChain_interceptors.setAccessible(true);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }
}