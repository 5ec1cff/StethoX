package io.github.a13e300.tools

import android.os.Handler
import android.os.Looper
import android.os.Message
import android.os.MessageQueue
import de.robv.android.xposed.XC_MethodHook
import de.robv.android.xposed.XposedBridge
import de.robv.android.xposed.XposedHelpers
import io.github.a13e300.tools.utils.ConcurrentIdentityWeakHashMap
import java.lang.reflect.Member
import java.util.Stack
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.atomic.AtomicInteger

data class Event(
    val type: String,
    val prev: Event?,
    val stack: Throwable = Throwable(),
)

val _eventStack = ThreadLocal<Stack<Event?>>()
val eventStack: Stack<Event?>
    get() =
        _eventStack.get() ?: Stack<Event?>().also { _eventStack.set(it) }

val association = ConcurrentIdentityWeakHashMap<Any, Event>()
val hookLock = Any()
var isHooked = false
val hookList = mutableListOf<XC_MethodHook.Unhook>()
val refCount = AtomicInteger()

fun enterAsyncTrace() {
    if (refCount.getAndIncrement() == 0) {
        installAsyncTraceHook()
    }
}

fun exitAsyncTrace() {
    if (refCount.decrementAndGet() == 0) {
        uninstallAsyncTraceHook()
    }
}

fun asyncStackTrace() = StringBuilder().apply {
    var event = eventStack.lastOrNull()
    while (event != null) {
        append("(async ")
        append(event.type)
        append(")\n")
        val stackTrace = event.stack.stackTrace
        val start =
            stackTrace.indexOfFirst { it.className == "LSPHooker_" }.let { if (it >= 0) it else 0 }
        for (j in start until stackTrace.size) {
            append(stackTrace[j])
            append("\n")
        }
        event = event.prev
    }
}.toString()

private val deopt by lazy {
    runCatching {
        XposedBridge::class.java.getDeclaredMethod("deoptimizeMethod", Member::class.java)
    }.onFailure {
        Logger.e("find deoptimizeMethod")
    }.getOrNull()
}

fun deoptimizeAll(clz: Class<*>, name: String) {
    clz.declaredMethods.forEach {
        if (it.name == name) {
            deopt?.invoke(null, it)
            Logger.d("deoptimized $it")
        }
    }
}

fun installAsyncTraceHook() {
    synchronized (hookLock) {
        if (isHooked) {
            Logger.d("repeatedly install async trace hook?")
            return
        }

        runCatching {
            hookList.add(XposedHelpers.findAndHookMethod(
                MessageQueue::class.java,
                "enqueueMessage",
                Message::class.java, java.lang.Long.TYPE,
                object : XC_MethodHook() {
                    override fun beforeHookedMethod(param: MethodHookParam) {
                        val msg = param.args[0] as Message
                        val key = msg.callback ?: return

                        val last = eventStack.lastOrNull()
                        val event = Event("HandlerPostRunnable", prev = last)
                        association[key] = event
                    }
                }
            ))

            hookList.add(XposedHelpers.findAndHookMethod(
                Handler::class.java,
                "dispatchMessage",
                Message::class.java,
                object : XC_MethodHook() {
                    override fun beforeHookedMethod(param: MethodHookParam) {
                        val msg = param.args[0] as Message
                        val key = msg.callback
                        if (key != null) {
                            val event = association.remove(key)
                            if (event != null) {
                                eventStack.push(event)
                                return
                            }
                        }
                        eventStack.push(null)
                    }

                    override fun afterHookedMethod(param: MethodHookParam) {
                        eventStack.pop()
                    }
                }
            ))

            val workerClass = XposedHelpers.findClass("java.util.concurrent.ThreadPoolExecutor\$Worker", ClassLoader.getSystemClassLoader())

            hookList.add(XposedHelpers.findAndHookMethod(
                Thread::class.java,
                "start",
                object : XC_MethodHook() {
                    override fun beforeHookedMethod(param: MethodHookParam) {
                        val last = eventStack.lastOrNull { it != null }
                        val target = XposedHelpers.getObjectField(
                            param.thisObject,
                            "target"
                        ) as? Runnable ?: return
                        if (workerClass.isInstance(
                                target
                            )
                        ) {
                            return
                        }

                        val event = Event("ThreadStart", last)
                        XposedHelpers.setObjectField(param.thisObject, "target", Runnable {
                            eventStack.push(event)
                            try {
                                target.run()
                            } finally {
                                eventStack.pop()
                            }
                        })
                    }
                }
            ))

            /*
            // TODO: support override Thread
            hookList.add(XposedHelpers.findAndHookMethod(
                Thread::class.java,
                "run",
                object : XC_MethodHook() {
                    override fun beforeHookedMethod(param: MethodHookParam) {
                        val key = param.thisObject
                        val event = association.remove(key) ?: return

                        eventStack.push(event)
                    }

                    override fun afterHookedMethod(param: MethodHookParam) {
                        eventStack.pop()
                    }
                }
            ))*/

            /*
            hookList.addAll(XposedBridge.hookAllConstructors(
                FutureTask::class.java,
                object : XC_MethodHook() {
                    override fun beforeHookedMethod(param: MethodHookParam) {
                        val key = param.thisObject
                        val last = eventStack.lastOrNull { it != null }
                        association[key] = Event("FutureTask", last)
                    }
                }
            ))

            hookList.add(XposedHelpers.findAndHookMethod(
                FutureTask::class.java,
                "run",
                object : XC_MethodHook() {
                    override fun beforeHookedMethod(param: MethodHookParam) {
                        val key = param.thisObject
                        val event = association.remove(key) ?: return

                        eventStack.push(event)
                    }

                    override fun afterHookedMethod(param: MethodHookParam) {
                        eventStack.pop()
                    }
                }
            ))*/

            hookList.add(XposedHelpers.findAndHookMethod(
                ThreadPoolExecutor::class.java,
                "execute",
                Runnable::class.java,
                object : XC_MethodHook() {
                    override fun beforeHookedMethod(param: MethodHookParam) {
                        val key = param.args[0] as? Runnable ?: return
                        val last = eventStack.lastOrNull { it != null }
                        val event = Event("ThreadPoolExecute", prev = last)
                        association[key] = event
                    }
                }
            ))

            hookList.add(XposedHelpers.findAndHookMethod(
                ThreadPoolExecutor::class.java,
                "runWorker",
                workerClass,
                object : XC_MethodHook() {
                    override fun beforeHookedMethod(param: MethodHookParam) {
                        val firstTask = XposedHelpers.getObjectField(param.args[0], "firstTask")
                        if (firstTask != null) {
                            val event = association.remove(firstTask)
                            if (event != null) {
                                eventStack.push(event)
                                return
                            }
                        }
                        eventStack.push(null)
                    }

                    override fun afterHookedMethod(param: MethodHookParam) {
                        eventStack.pop()
                    }
                }
            ))

            val getTaskTls = ThreadLocal<Boolean>()
            hookList.add(XposedHelpers.findAndHookMethod(
                ThreadPoolExecutor::class.java,
                "getTask",
                object : XC_MethodHook() {
                    override fun beforeHookedMethod(param: MethodHookParam) {
                        if (getTaskTls.get() == true) {
                            eventStack.pop()
                        } else {
                            getTaskTls.set(true)
                        }
                    }

                    override fun afterHookedMethod(param: MethodHookParam) {
                        val result = param.result ?: return
                        val event = association.remove(result)
                        if (event != null) {
                            eventStack.push(event)
                            return
                        }
                        eventStack.push(null)
                    }
                }
            ))

            deoptimizeAll(Looper::class.java, "loop")
            deoptimizeAll(Looper::class.java, "loopOnce")
            deoptimizeAll(Handler::class.java, "post")
            deoptimizeAll(Handler::class.java, "postDelayed")
            deoptimizeAll(Handler::class.java, "postAtTime")
            deoptimizeAll(Handler::class.java, "sendMessageAtTime")
            deoptimizeAll(Handler::class.java, "sendMessageDelayed")
            deoptimizeAll(Handler::class.java, "enqueueMessage")
            Logger.d("async hook installed")
        }.onFailure {
            Logger.e("install async trace hook")
        }
        isHooked = true
    }
}

fun uninstallAsyncTraceHook() {
    synchronized(hookLock) {
        if (!isHooked) {
            return
        }
        hookList.forEach {
            runCatching {
                it.unhook()
            }
        }
        hookList.clear()
        isHooked = false
    }
}
