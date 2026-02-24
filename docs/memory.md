# 内存扫描

## `hook.getObjectsOfClass`

```
hook.getObjectsOfClass(class[, subClasses])
```

获取内存中类型为指定类的所有对象，class 是类对象，可以使用 `hook.findClass` 获取。
可选值 subClasses 接受一个 bool ，表示是否同时获取类型为该类的子类的对象，默认为 false 。
返回所需要对象的列表。

该方法使用 JVMTI 实现，可能不兼容早期 Android 版本。

## `hook.getAssignableClasses`

```
hook.getAssignableClasses(class)
hook.getAssignableClasses(className[, classLoader])
```

获取指定类的所有子类（即对于类 A ，获取所有满足 `A.isAssignableFrom(B)` 的类 B）。
可以提供 class 对象，也可以提供类名或类加载器（如无类加载器，则使用 hook 对象上的默认类加载器）

## `hook.findPathToObject`

```
hook.findPathToObject(
  obj, 
  { 
    maxTries?, 
    maxDepth?, 
    object, 
    class, 
    cond(obj) -> boolean {} 
  }
)
```

从指定对象 `obj` 开始扫描满足条件的对象，返回到达满足条件的对象的路径。路径可使用 .toString 美观输出。

maxTries: 最大尝试次数，默认为 Int.MAX

maxDepth: 最大搜索深度，默认为 Int.MAX ，建议实际使用时先尝试较小的数值

object: 要扫描的目标对象的引用。如果提供，则遇到引用相等的对象时输出一条路径。

class: 要扫描的目标对象的类型。如果提供，则遇到类型为该类（包括子类）的对象时输出一条路径。

cond: 回调函数，用于确定遇到的对象是否为目标对象。如果提供，则会调用该函数，参数为遇到的对象，应该返回布尔值，true 表示是目标对象。

返回值：到达目标对象的所有路径，如未查找到任何路径则抛出异常。
