StethoX
=======

## Description

Original Project: https://gitlab.com/derSchabi/Stethox

This is a [Xposed](http://repo.xposed.info/module/de.robv.android.xposed.installer) Module 
that enables [Stetho](https://github.com/5ec1cff/stetho) ([origin](https://facebook.github.io/stetho/)) for every application on your phone,
allows you inspect any application with Chrome Remote Devtools.

## Usage

1. Install Xposed and this module.  
2. Enable this module for apps you need.  
3. Connect to your PC via USB Debugging (ADB).  
4. Open chrome://inspect , select your app to inspect.  
5. Have fun !  

## Features

- View the View tree like an element (supports Activity, Dialog or other Window).
- Screen mirroring and visual element selection (you need to select the required view in Elements to mirror).
- Use Javascript in the console to access Java objects and hook Java methods (type `hook.help()` to view help).
- Support for suspending process at startup

## Suspend process

StethoX supports suspending the App process when it starts and displays the "Waiting for Debugger" dialog. When the user connects with Devtools, he can enter `cont` at the console to keep the process running.

Before the process continues to run, the process can also be accessed through Devtools, such as executing Javascript and other operations.

**NOTE: You may need to turn off StethoX's battery optimization for this feature to work properly. **

You can configure suspend using the `content` command in the adb shell:

```shell
# Suspend once (take effect at next startup)
content call --uri content://io.github.a13e300.tools.stethox.suspend --method suspend --arg ${processName}
#Hang all the time
content call --uri content://io.github.a13e300.tools.stethox.suspend --method suspend_forever --arg ${processName}
# Clear configuration (clear all without specifying arg)
content call --uri content://io.github.a13e300.tools.stethox.suspend --method clear [--arg ${processName}]
# List configuration
content call --uri content://io.github.a13e300.tools.stethox.suspend --method list
```

You can also configure it by prop or file:

```
# Separate with newline
setprop debug.stethox.suspend <processname>[,<processname>...]
# clear
setprop debug.stethox.suspend ''

# Separate with newline
echo '<processname>' > /data/local/tmp/stethox_suspend
# clear
rm /data/local/tmp/stethox_suspend
```

## ATTENTION

Never leave this Module enabled or installed on day to day use.
__THIS IS A SECURITY RISK__. Only enable this for Development.

---

## 描述

原项目： https://gitlab.com/derSchabi/Stethox

这是一个能够为任意应用启用 [Stetho](https://github.com/5ec1cff/stetho) ([原 facebook 项目](https://facebook.github.io/stetho/))
的 [Xposed](http://repo.xposed.info/module/de.robv.android.xposed.installer) 模块，你可以借助它通过 Chrome 开发者工具检查任意 App 。 

## 用法

1. 安装 Xposed 及该模块。  
2. 为你需要的应用启用模块。  
3. 通过 USB 调试 (ADB) 连接到电脑。  
4. 打开 chrome://inspect ，选择要检查的 App 。  
5. 祝你玩得开心！  

## 功能

- 像审查元素一样查看 View 树（支持 Activity、Dialog 或其他 Window）。  
- 画面镜像和可视的元素选择（需要在 Elements 中选中需要的 view 以镜像）。  
- 在控制台使用 Javascript 访问 Java 对象和 hook Java 方法（输入 `hook.help()` 查看帮助）。  
- 支持启动时挂起进程  

## 挂起进程

StethoX 支持在 App 进程启动时挂起进程，并显示「等待调试器」对话框。当用户使用 Devtools 连接后，可以在控制台输入 `cont` 让进程继续运行。

在进程继续运行之前，也可以通过 Devtools 访问进程，如执行 Javascript 等操作。

**注意：你可能需要关闭 StethoX 的电池优化确保该功能正常工作。**

你可以在 adb shell 使用 `content` 命令配置挂起：

```shell
# 挂起一次（下次启动时生效）
content call --uri content://io.github.a13e300.tools.stethox.suspend --method suspend --arg ${processName}
# 一直挂起
content call --uri content://io.github.a13e300.tools.stethox.suspend --method suspend_forever --arg ${processName}
# 清除配置（不指定 arg 则清除所有）
content call --uri content://io.github.a13e300.tools.stethox.suspend --method clear [--arg ${processName}]
# 列出配置
content call --uri content://io.github.a13e300.tools.stethox.suspend --method list
```

你也可以使用 prop 或者文件配置：

```
# Separate with newline
setprop debug.stethox.suspend <processname>[,<processname>...]
# clear
setprop debug.stethox.suspend ''

# Separate with newline
echo '<processname>' > /data/local/tmp/stethox_suspend
# clear
rm /data/local/tmp/stethox_suspend
```

## 注意

日常使用请勿启用该模块，这样有安全风险，请只在开发时启用。
