package io.github.a13e300.tools.network.okhttp3;

import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Field;

public interface OkHttp3Helper {
    Object /*Request*/ Interceptor$Chain_request(Object chain);
    Object /*Response*/ Interceptor$Chain_proceed(Object chain, Object /*Request*/ request) throws IOException;
    Object /*Connection*/ Interceptor$Chain_connection(Object chain);


    Object /*ResponseBody*/ Response_body(Object response);
    String Response_header(Object response, String name);
    Object /*Headers*/ Response_headers(Object response);
    Object /*Response*/ Response_cacheResponse(Object response);
    Object /*Response$Builder*/ Response_newBuilder(Object response);
    Object /*Response*/ Response$Builder_build(Object builder);
    /*static*/ Object /*ResponseBody*/ ResponseBody_create(Object /*MediaType*/ contentType, long contentLength, Object /*BufferedSource*/ content);
    Object /*Response$Builder*/ Response$Builder_body(Object builder, Object /*ResponseBody*/ body);


    Object /*MediaType*/ ResponseBody_contentType(Object body);
    InputStream ResponseBody_byteStream(Object body);
    long ResponseBody_contentLength(Object body);

    Object /*HttpUrl*/ Request_url(Object request);
    String Request_method(Object request);
    Object /*RequestBody*/ Request_body(Object request);
    Object /*Headers*/ Request_headers(Object request);
    String Request_header(Object request, String name);

    void RequestBody_writeTo(Object body, Closeable /*BufferedSink*/ bufferedSink) throws IOException;

    int Headers_size(Object headers);
    String Headers_name(Object headers, int index);
    String Headers_value(Object headers, int index);

    int Response_code(Object response);
    String Response_message(Object response);

    /*static*/ Closeable /*Sink*/ Okio_sink(OutputStream out);
    /*static*/ Closeable /*BufferedSink*/ Okio_buffer(Closeable /*Sink*/ sink);
    /*static*/ Closeable /*Source*/ Okio_source(InputStream in);
    /*static*/ Closeable /*BufferedSource*/ Okio_1buffer(Closeable /*Source*/ source);



    Class Interceptor();

    Class okhttp3_internal_http_RealInterceptorChain();
    Field okhttp3_internal_http_RealInterceptorChain_interceptors();
}
