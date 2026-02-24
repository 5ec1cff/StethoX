import tkinter as tk
from tkinter import ttk
import threading
import time
import subprocess as sp
import httpx
import webbrowser
import re
import json
import time
from urllib.parse import urlencode
import urllib.request

ADB = 'adb'

def exec_adb(p, device=None):
    args = [ADB]
    if device is not None:
        args += ['-s', device]
    args += p
    proc = sp.Popen(args, stdout=sp.PIPE)
    data = proc.stdout.read()
    return proc.wait(), data

def scan_unix(device=None):
    _, d = exec_adb(['shell', 'grep @stetho_ /proc/net/unix'], device=device)
    content = d.decode('utf-8')
    results = []
    for line in content.split('\n'):
        tokens = line.strip().split(' ')
        if len(tokens) < 8:
            continue
        if tokens[5] != '01':
            # listen
            print('not listening:', tokens[5])
            continue
        path = tokens[7]
        if path.startswith('@stetho_'):
            results.append(path.removeprefix('@'))

    return results

def forward_socket(name, device):
    _, n = exec_adb(['forward', '--list'], device=device)
    content = n.decode('utf-8')
    for l in content.split('\n'):
        tokens = l.strip().split(' ')
        if tokens[0] != device:
            continue
        if not tokens[1].startswith('tcp:'):
            continue
        if tokens[2] == 'localabstract:' + name:
            return tokens[1].removeprefix('tcp:')
    # not found, alloc new one
    _, n = exec_adb(['forward', 'tcp:0', f'localabstract:{name}'])
    return n.decode('utf-8').strip()

STETHO_DEVTOOLS_URL = "http://chrome-devtools-frontend.appspot.com/serve_rev/@040e18a49d4851edd8ac6643352a4045245b368f/inspector.html"

def get_service_info(name, device=None):
    try:
        port = forward_socket(name, device)
        print(port, name)
        t = time.time()
        request = urllib.request.Request(f'http://localhost:{port}/json/list')
        with urllib.request.urlopen(request, timeout=5) as response:
            d = time.time() - t
            print('duration:', d)
            response_data = response.read()
            data = json.loads(response_data.decode('utf-8'))
        d = time.time() - t
        print('duration:', d, data)
        data = data[0]
        title = data['title'].replace(' (powered by Stetho)', '')
        url:str = data['devtoolsFrontendUrl']
        url = url[:url.index('?')] + '?' + urlencode({'ws': f'localhost:{port}/inspector'})
        stetho_url:str = data['url']
        pid, name, = stetho_url.removeprefix('stetho://').split('/')
        return url,title,port,pid,name
    except:
        from traceback import print_exc
        print_exc()
        return None

def list_devices():
    r = []
    _, n = exec_adb(['devices'])
    for l in n.decode('utf-8').split('\n')[1:]:
        tokens = re.split(r'\s+', l.strip())
        if len(tokens) != 2:
            continue
        if tokens[1] != 'device':
            continue
        r.append(tokens[0])
    return r

PLACEHOLDER = '正在获取……'

class StethoXApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("StethoX 连接器")
        self.root.geometry("800x500")

        main_frame = ttk.Frame(root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        root.columnconfigure(0, weight=1)
        root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)
        main_frame.rowconfigure(1, weight=1)

        button_frame = ttk.Frame(main_frame)
        button_frame.grid(row=0, column=0, pady=(0, 10), sticky=tk.W)

        self.fetch_button = ttk.Button(
            button_frame,
            text="刷新",
            command=self.fetch_services
        )
        self.fetch_button.grid(row=0, column=0, padx=5)

        self.open_service_button = ttk.Button(
            button_frame,
            text="打开服务",
            command=self.open_btn_clicked
        )
        self.open_service_button.grid(row=0, column=1, padx=5)

        self.status_label = ttk.Label(button_frame, text="")
        self.status_label.grid(row=0, column=4, padx=20)

        table_frame = ttk.Frame(main_frame)
        table_frame.grid(row=1, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        scrollbar_y = ttk.Scrollbar(table_frame, orient=tk.VERTICAL)
        scrollbar_x = ttk.Scrollbar(table_frame, orient=tk.HORIZONTAL)

        self.table = ttk.Treeview(
            table_frame,
            columns=("Device", "App", "PID", "Proc", "Socket", "Port"),
            show="headings",
            yscrollcommand=scrollbar_y.set,
            xscrollcommand=scrollbar_x.set
        )

        scrollbar_y.config(command=self.table.yview)
        scrollbar_x.config(command=self.table.xview)

        self.table.heading("Device", text="设备")
        self.table.heading("App", text="App")
        self.table.heading("PID", text="进程 ID")
        self.table.heading("Proc", text="进程名")
        self.table.heading("Socket", text="套接字名")
        self.table.heading("Port", text="本地 TCP 端口")

        self.table.column("Device", width=60, anchor=tk.CENTER)
        self.table.column("App", width=80, anchor=tk.CENTER)
        self.table.column("PID", width=60, anchor=tk.CENTER)
        self.table.column("Proc", width=100, anchor=tk.CENTER)
        self.table.column("Socket", width=100, anchor=tk.CENTER)
        self.table.column("Port", width=100, anchor=tk.CENTER)

        self.table.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        scrollbar_y.grid(row=0, column=1, sticky=(tk.N, tk.S))
        scrollbar_x.grid(row=1, column=0, sticky=(tk.W, tk.E))

        self.table.bind('<Double-1>', self.open_service)
        self.table.bind('<Return>', self.open_service)

        table_frame.columnconfigure(0, weight=1)
        table_frame.rowconfigure(0, weight=1)

        self.table.bind('<<TreeviewSelect>>', self.on_row_select)
        self.open_service_button.config(state='disabled')
        self.device_count = 0
        self.fetch_services()

    def open_service(self, _):
        selected_items = self.table.selection()
        if selected_items:
            port = self.table.item(selected_items[0])['values'][5]
            url = STETHO_DEVTOOLS_URL + '?' + urlencode({'ws': f'localhost:{port}/inspector'})
            webbrowser.open(url)


    def fetch_services(self):
        self.fetch_button.config(state='disabled')
        self.status_label.config(text="正在拉取数据...")

        thread = threading.Thread(target=self.fetch_services_thread, daemon=True)
        thread.start()

    def update_service_info(self, device, name, title, pid, proc):
        for k in self.table.get_children():
            item = self.table.item(k)
            values = item['values']
            if values[0] == device and values[4] == name:
                print('found item', k)
                self.table.set(k, 1, title)
                self.table.set(k, 2, pid)
                self.table.set(k, 3, proc)
                return
        print('no item found')

    def fetch_service_info(self, device, name):
        service_info = get_service_info(name, device)
        if service_info is not None:
            _, title, _, pid, proc = service_info
            self.root.after(0, self.update_service_info, device, name, title, pid, proc)

    def fetch_services_thread(self):
        devices = list_devices()
        self.device_count = len(devices)
        data = []
        for device in devices:
            ls = scan_unix(device)
            if len(ls) == 0:
                print('no stethox service found on', device)
            for name in ls:
                port = forward_socket(name, device)

                thread = threading.Thread(target=self.fetch_service_info, daemon=True, args=(device, name, ))
                thread.start()
                data.append((device, PLACEHOLDER, PLACEHOLDER, PLACEHOLDER, name, port))

        self.root.after(0, self.update_table, data)

    def update_table(self, data):
        self.clear_table()

        for row in data:
            self.table.insert("", tk.END, values=row)

        self.fetch_button.config(state='normal')
        if self.device_count > 0:
            l = len(data)
            if l > 0:
                self.status_label.config(text=f"{self.device_count} 个设备已连接，共 {len(data)} 个服务")
            else:
                self.status_label.config(text=f"{self.device_count} 个设备已连接，未发现服务")
        else:
            self.status_label.config(text=f"没有设备连接，请接入设备后刷新")

    def open_btn_clicked(self):
        self.open_service(None)

    def clear_table(self):
        for item in self.table.get_children():
            self.table.delete(item)

    def on_row_select(self, event):
        selected_items = self.table.selection()
        if selected_items:
            self.open_service_button.config(state='enabled')
        else:
            self.open_service_button.config(state='disabled')


if __name__ == "__main__":
    root = tk.Tk()
    app = StethoXApp(root)
    root.mainloop()
