# 启动

StethoX 模块生效后，app 会启动 Stetho 服务，此时有三种方法可以连接：

## 使用 stethox-launcher.py （推荐）

项目根目录下有 stethox-launcher.py ，需要 python3 和支持 tkinter 图形界面的环境，以及 PATH 中需要存在 adb ，
执行脚本即可自动扫描连接到的设备上的 StethoX 实例，并打开浏览器进行连接。

## Chrome 系浏览器自动检测（不推荐）

Stetho 实现了 Chrome Devtools Protocol ，因此可以被 Chrome 浏览器自动检测，具体可参考[这里](https://developer.chrome.com/docs/devtools/remote-debugging)

简单来说，打开 chrome://inspect 并勾选「Discover USB Devices」，用 adb 连接到手机

然而 Chrome 扫描速度非常慢，且容易遇到卡死问题。 对于较新版本的 Android 系统，一般都有冻结机制（即不在前台的 app 进程被完全停止运行），
导致 Stetho 无法响应 Unix Socket 的连接，而 Chrome 会对扫描到的 socket 不停发送连接请求，直到占满 adb 的连接数量，
此时任何 adb 命令都无法执行，只能重启 adbd 。 因此，不建议使用 Chrome 的自动检测，应该使用其他两种方法进行连接。


## 手动连接

如果你无法使用以上方法进行连接，在只有 adb 的情况下可以手动连接（你也可以参考 stethox-launcher.py 的代码）：

```sh
# 进入 adb shell
adb shell
# 查找设备上的 stethox 实例
grep stetho /proc/net/unix
# 可以达到类似以下的输出结果，可能有多条
# unix socket 名称为 "stetho_进程名_devtools_remote[_pid]"
# 这里的 "@" 表示是一个 abstract socket，无需输入到 adb forward 中
0000000000000000: 00000002 00000000 00010000 0001 01 278342952 @stetho_org.telegram.messenger_devtools_remote
# 在另一个 shell 中进行端口转发
# adb forward LOCAL REMOTE
# LOCAL 为 tcp:9222 即转发到本地的 tcp 端口 9222
# REMOTE 是你想要连接到的 stetho 实例的 socket 名字 
adb forward tcp:9222 unix:stetho_org.telegram.messenger_devtools_remote
# 接下来打开以下链接即可进入 Chrome Devtools
# 其中参数 ws=localhost:本地 TCP 端口号/inspector 根据你的需求进行修改，并将 "/" 转换成 %2F
http://chrome-devtools-frontend.appspot.com/serve_rev/@040e18a49d4851edd8ac6643352a4045245b368f/inspector.html?ws=localhost:9222%2finspector
# 你也可以访问 localhost:9222/json 获取 app 名称、进程名称、pid 等信息
```

[Chrome Devtools Protocol](https://chromedevtools.github.io/devtools-protocol/)
