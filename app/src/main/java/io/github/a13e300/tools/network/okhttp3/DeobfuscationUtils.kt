package io.github.a13e300.tools.network.okhttp3

import io.github.a13e300.tools.DexKitWrapper
import io.github.a13e300.tools.deobfuscation.MutableNameMap
import io.github.a13e300.tools.deobfuscation.NameMap

fun deobfOkhttp3(cl: ClassLoader): NameMap {
    val map = MutableNameMap()
    DexKitWrapper.create(cl).use {
        val classRealInterceptorChain = it.findClass {
            matcher {
                usingStrings = listOf("must call proceed() exactly once", "returned a response with no body")
            }
        }
        require(classRealInterceptorChain.size == 1) { "require only one RealInterceptorChain, found ${classRealInterceptorChain.size}" }
        val fieldInterceptors = classRealInterceptorChain[0].fields.findField {
            matcher {
                type = "java.util.List"
            }
        }
        require(fieldInterceptors.size == 1) { "require only one Interceptors field, found ${fieldInterceptors.size}" }
        map.putClass("okhttp3.internal.http.RealInterceptorChain", classRealInterceptorChain[0].name)
        map.putField("okhttp3.internal.http.RealInterceptorChain", "interceptors", fieldInterceptors[0].name)
    }
    return map
}
