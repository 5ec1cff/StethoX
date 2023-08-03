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

- Inspect view (Support activities, dialogs and other windows)  
- View mirror  
- Access Java objects and hook Java methods with Rhino Javascript REPL in console (Use `hook.help()` for help)  

The License is GPL-3.0-or-later

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

## 注意

日常使用请勿启用该模块，这样有安全风险，请只在开发时启用。
