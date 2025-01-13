# dual
路由器的科学上网透明网关。每天自动更新国内全部IP段数据和domain数据，对于任何国内流量都默认走国内线路，国外流量转发机场。

# 特点
* 对内网设备透明，网络终端（phone/pc/pad/tv）无需任何设置。
* 支持任意网络终端的科学上网能力，任何在TCP/UDP上的协议流量都会透明科学上网。
* 对外网采用WebSocket Over SSL 伪装成正常Web流量，减少被探测概率。
  
# build

`apt install libcurl-openssl-dev`  

`apt install libsqlite3`  

`add-apt-repository ppa:maxmind/ppa && apt update`  

`apt install libmaxminddb0 libmaxminddb-dev mmdb-bin`

`cd dual && bash build.sh`

# 部署环境
* 路由器 DHCP 默认网关设置为 dual 所在设备的内网IP
* dual 所在设备和外网机场需要运行 [wssocks5](https://github.com/yingshulu/wssocks5)

# 启动
```sudo ./startup.sh ```
