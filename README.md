# Civil fixed-wing avionics lab / 民用固定翼航电台架

## 中文

59 个 C 应用服务分配到六个逻辑板卡角色，包含合成飞控闭环、主备姿态、导航、机电、气象和座舱。Linux 默认使用 Cyclone DDS，支持 UDP 对照。中文座舱只读订阅经模拟传感器和应用处理的数据，显示过期时清空数值。Genann 气象是旁路，不计入 59 个服务。

- Ubuntu 22.04 安装 `gcc libc6-dev cyclonedds-dev cyclonedds-tools`，运行 `bash build.sh && bash check_offline.sh`。
- `bash tools/preview_panel.sh 30`：运行 30 秒本地 DDS 台架；浏览器打开命令打印的 `index.html`，查看实时座舱。
- `bash check_loopback.sh dds`：联测并生成抓包、显示快照和只读健康检查结果。用 `udp` 参数做对照。
- Windows 可运行 `启动研究平台.ps1` 生成全容器计划，或用 `-Mode process` 生成全进程计划。可用 `-BuildOnly` 在 WSL 中编译和检查。

## English

59 C application services map to six logical boards. They cover a synthetic flight-control loop, dual attitude inputs, navigation, aircraft systems, weather, and the flight deck. Linux uses Cyclone DDS by default, with a UDP comparison mode. The Chinese flight deck subscribes to processed synthetic sensor outputs and blanks stale values. The Genann weather advisory is separate from the 59 services.

- On Ubuntu 22.04, install `gcc libc6-dev cyclonedds-dev cyclonedds-tools` and run `bash build.sh && bash check_offline.sh`.
- `bash tools/preview_panel.sh 30`: run a 30-second local DDS bench and open the printed `index.html` in a browser.
- `bash check_loopback.sh dds`: verify the running data paths, display snapshots, and passive health reader. Use `udp` for comparison.
- The Windows PowerShell launcher prints an all-container or all-process plan. `-BuildOnly` builds and checks under WSL.
