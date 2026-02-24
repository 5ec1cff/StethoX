# Hook

## hook 函数

```
hook(
    class|className[, classLoader], 
    methodName[, parameterTypes|null],
    callback(param: HookParam) {}
)
HookParam {
    thisObject
    args
    result
    throwable
    method
    invoke()
}
```

注册 hook ，可以用 class 对象或者 className + classLoader 的组合来指定要 hook 的类（如果未提供 classLoader 则
使用 hook 对象的默认 classLoader）。使用 methodName 和 parameterTypes 来确定类中要 hook 的方法。可以是逗号分隔的
一些特殊值 `@` 加上 `all(ALL,method),constructor(init),public,instance` ，分别表示：所有方法、所有构造器、
所有公开方法、所有实例方法（即非静态方法）。参数类型是可选的，如未提供表示 hook 所有同名方法，否则表示匹配指定类型的方法；
如果只要匹配参数列表为空的方法，则可用一个 null 表示。

callback 是在被 hook 方法被执行前执行的函数，有一个参数 param ，可以获得 HookParam 对象，可以获取当前被 hook 方法的调用
的 this 对象(thisObject) ，获取和修改参数列表(args)，直接设置结果(result)或异常(throwable)，获取被 hook 方法对象(method)；
调用 `.invoke()` 可以（多次）执行原方法（使用 HookParam 中被修改的参数列表），从而获取返回值或者异常（结果会从这里返回，异常会从这里抛出）。
如果未执行 `.invoke()` ，则回调函数返回后会自动执行原方法，可以用 `param.result = null` 阻止原方法被自动执行。

## 跟踪(`hook.trace(s)`)

```
hook(
    class|className[, classLoader], 
    methodName[, parameterTypes|null],
    { 
        cond(param: HookParam): Boolean {}?,
        preCond(param: HookParam): Boolean {}?
    }?
)
```

trace 函数与 hook 函数指定要 hook 的方法的方式类似，只是会自动打印参数、this 对象。
可提供可选的 cond 函数和 preCond 函数，分别表示在方法完成后和完成前判断是否需要输出本次跟踪结果。
回调函数返回 true 表示需要输出本次跟踪，如为提供则默认为 true 。

traces 与 trace 相同，只是增加了调用栈输出。

## 异步跟踪(`hook.atraces`)

该函数的作用与 traces 相同，但是同时启用了异步跟踪功能，具体而言就是在打印调用堆栈后追加异步调用的堆栈，
如 `Handler.post` , `ThreadPoolExecutor.execute` , `Thread.start` 等异步调用。

## unhook

hook 和 trace 函数会返回一个 unhook 对象，在其上调用 `.unhook` 可停止 hook

也可以使用 `hook.clear()` 移除当前注册的所有 hook 。
