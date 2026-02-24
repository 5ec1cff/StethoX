package io.github.a13e300.tools

import com.facebook.stetho.rhino.JsConsole
import io.github.a13e300.tools.objects.HookFunction
import org.mozilla.javascript.Scriptable
import java.lang.ref.WeakReference
import java.lang.reflect.Field
import java.lang.reflect.Member
import java.lang.reflect.Modifier
import java.util.IdentityHashMap
import java.util.LinkedList

sealed class Edge(open val obj: Any, open val prev: Edge?, val len: Int = prev?.len?.inc() ?: 0) {
    data class StartEdge(override val obj: Any) : Edge(obj, null) {
        override fun toString(): String {
            return toStringSafe(newLine = true)
        }
    }

    data class FieldEdge(override val obj: Any, override val prev: Edge?, val field: Field) : Edge(obj, prev) {
        override fun toString(): String {
            return toStringSafe(newLine = true)
        }
    }

    data class ArrayIndexEdge(override val obj: Any, override val prev: Edge?, val index: Int) : Edge(obj, prev) {
        override fun toString(): String {
            return toStringSafe(newLine = true)
        }
    }

    data class WeakRefEdge(override val obj: Any, override val prev: Edge?) : Edge(obj, prev) {
        override fun toString(): String {
            return toStringSafe(newLine = true)
        }
    }

}

fun Edge.toStringSafe(limit: Int = -1, newLine: Boolean = false): String {
    var edge: Edge? = this
    var i = 0
    return StringBuilder().apply {
        append("Path{len=")
        append(this@toStringSafe.len)
        append(" ")
        while (edge != null) {
            if (i != 0) {
                append(" -> ")
            }
            if (limit > 0 && i >= limit) {
                append(" ... ")
                break
            }
            if (newLine) {
                append("\n  ")
            }
            when (edge) {
                is Edge.StartEdge -> {
                    append(" at Start")
                }
                is Edge.FieldEdge -> {
                    append(" at Field ")
                    append(edge.field)
                }
                is Edge.ArrayIndexEdge -> {
                    append(" at Array ")
                    append(edge.index)
                }
                is Edge.WeakRefEdge -> {
                    append(" at WeakRef")
                }
            }
            i += 1
            edge = edge.prev
        }
        if (newLine) {
            append("\n")
        }
        append("}")
    }.toString()
}

data class FindObjectConfiguration(
    val cond: ((Any) -> Boolean)?,
    val maxDepth: Int,
    val maxTries: Int,
    val targetObj: Any?,
    val targetClz: Class<*>?,
)

fun findPathToObject(start: Any, conf: FindObjectConfiguration) : List<Edge> {
    val visited = IdentityHashMap<Any, Boolean>()
    val visiting = LinkedList<Edge>()
    var cnt = 0
    val result = mutableListOf<Edge>()

    fun isValid(o: Any): Boolean {
        return !visited.contains(o) && !(o is Int
            || o is Boolean
            || o is Long
            || o is Char
            || o is Float
            || o is Double
            || o is Short
            || o is Byte
            || o is IntArray
            || o is BooleanArray
            || o is LongArray
            || o is CharArray
            || o is FloatArray
            || o is DoubleArray
            || o is ShortArray
            || o is ByteArray
            || o is String
            || o is Class<*>
            || o is Member
            || o is ClassLoader
        ) && o.javaClass.classLoader != HookFunction::class.java.classLoader
    }

    fun matches(obj: Any): Boolean {
        return (conf.targetObj != null && obj === conf.targetObj)
                || (conf.targetClz != null && conf.targetClz.isInstance(obj))
                || (conf.cond?.invoke(obj) == true)
    }

    visiting.push(Edge.StartEdge(start))
    visited[start] = java.lang.Boolean.TRUE
    Logger.d("start scan from $start")

    while (visiting.isNotEmpty()) {
        val edge = visiting.pop()
        val o = edge.obj
        cnt += 1
        if (cnt >= conf.maxTries) {
            // Logger.d("visited cnt ${visited.size}")
            // visited.forEach { k, _ -> Logger.d("visited: $k ${System.identityHashCode(k)}") }
            // error("too many tries!!!")
            break
        }

        if (o is Array<*>) {
            for (i in 0 until o.size) {
                val newObj = o[i] ?: continue
                if (matches(newObj)) {
                    result.add(Edge.ArrayIndexEdge(newObj, edge, i))
                    continue
                }
                if (edge.len < conf.maxDepth && isValid(newObj)) {
                    visiting.push(Edge.ArrayIndexEdge(newObj, edge, i))
                    visited[newObj] = java.lang.Boolean.TRUE
                }
            }
        } else if (o is WeakReference<*>) {
            o.get()?.let { newObj ->
                if (matches(newObj)) {
                    result.add(Edge.WeakRefEdge(newObj, edge))
                    return@let
                }
                if (edge.len < conf.maxDepth && isValid(newObj)) {
                    visiting.push(Edge.WeakRefEdge(newObj, edge))
                    visited[newObj] = java.lang.Boolean.TRUE
                }
            }
        } else {
            var clz = o.javaClass
            while (clz != Object::class.java) {
                clz.declaredFields.forEach {
                    if (Modifier.isStatic(it.modifiers)) return@forEach
                    it.isAccessible = true
                    val newObj = it.get(o) ?: return@forEach
                    if (matches(newObj)) {
                        result.add(Edge.FieldEdge(newObj, edge, it))
                        return@forEach
                    }
                    if (edge.len < conf.maxDepth && isValid(newObj)) {
                        visiting.push(Edge.FieldEdge(newObj, edge, it))
                        visited[newObj] = java.lang.Boolean.TRUE
                    }
                }
                clz = clz.superclass
            }
        }
    }

    if (result.isEmpty()) {
        error("object not found (traversal $cnt objects)")
    }
    return result
}

sealed class ObjectDiff {
    data class TypeDifference(val left: Any?, val right: Any?) : ObjectDiff()
    data class ValueDifference(val left: Any?, val right: Any?) : ObjectDiff()
    data class StructDifference(val left: Any?, val right: Any?, val diff: Map<Field, ObjectDiff>) : ObjectDiff()
    data object NoDifference : ObjectDiff()
}

fun Class<*>.isPrimitiveType() =
    when (this) {
        Integer.TYPE, java.lang.Boolean.TYPE, java.lang.Long.TYPE, java.lang.Short.TYPE,
        Character.TYPE, java.lang.Double.TYPE, java.lang.Float.TYPE, java.lang.Byte.TYPE,
        Int::class.java, Boolean::class.java, Long::class.java, Short::class.java,
        Char::class.java, Double::class.java, Float::class.java, Byte::class.java,
        IntArray::class.java, BooleanArray::class.java, LongArray::class.java, ShortArray::class.java,
        CharArray::class.java, DoubleArray::class.java, FloatArray::class.java, ByteArray::class.java
            -> true

        else -> CharSequence::class.java.isAssignableFrom(this)
    }


fun diffObject(obj1: Any?, obj2: Any?, maxLevel: Int): ObjectDiff {
    assert(maxLevel >= 0)
    fun diffInternal(obj1: Any?, obj2: Any?, level: Int): ObjectDiff {
        if (obj1?.javaClass !== obj2?.javaClass) {
            return ObjectDiff.TypeDifference(obj1, obj2)
        }

        if (obj1 == null) return ObjectDiff.NoDifference
        if (level == maxLevel) return ObjectDiff.NoDifference

        var clz = obj1.javaClass
        val difference = mutableMapOf<Field, ObjectDiff>()
        while (clz != Object::class.java) {
            clz.declaredFields.forEach { field ->
                if (Modifier.isStatic(field.modifiers)) return@forEach
                field.isAccessible = true
                val f1 = field.get(obj1)
                val f2 = field.get(obj2)
                if (field.type.isPrimitiveType()) {
                    if (f1 != f2) {
                        difference[field] = ObjectDiff.ValueDifference(f1, f2)
                    }
                } else if (f1 !== f2) {
                    val diff = diffInternal(f1, f2, level + 1)
                    if (diff !== ObjectDiff.NoDifference) {
                        difference[field] = diff
                    }
                }
            }

            clz = clz.superclass
        }

        if (difference.isEmpty()) {
            return ObjectDiff.NoDifference
        } else {
            return ObjectDiff.StructDifference(obj1, obj2, difference)
        }
    }

    return diffInternal(obj1, obj2, 0)
}

fun diffObjectOnConsole(obj1: Any, obj2: Any, maxLevel: Int, scope: Scriptable) {
    val console = JsConsole.fromScope(scope)
    val diff = diffObject(obj1, obj2, maxLevel)
    fun logDifference(diff: ObjectDiff, prefix: String) {
        when (diff) {
            ObjectDiff.NoDifference -> {
                console.log("$prefix No difference")
            }

            is ObjectDiff.TypeDifference -> {
                console.log("$prefix Type different", diff.left, diff.right)
            }

            is ObjectDiff.ValueDifference -> {
                console.log("$prefix Value different", diff.left, diff.right)
            }

            is ObjectDiff.StructDifference -> {
                val newPrefix = "$prefix "
                console.log("$prefix Struct different", diff.left, diff.right)
                diff.diff.forEach { (field, diff) ->
                    console.log("${newPrefix} Struct different on field", field)
                    logDifference(diff, newPrefix)
                }
            }
        }
    }
    console.log("Diff ", obj1, obj2)
    logDifference(diff, "")
}
