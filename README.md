# Civil fixed-wing avionics lab / 固定翼航电仿真台架

## 中文

一个 Linux 上的民用固定翼 C 台架。操纵输入经过飞控、作动器和合成飞机模型，姿态反馈给飞控及主飞行显示；另有导航、动力、供电、油量、起落架、液压、环境和座舱状态。气象 Genann 小模型是独立旁路。飞机数据和控制响应均为合成。跨域使用静态采样，数据校验和路由与 Linux 收发分开。

- `bash build.sh && bash check_offline.sh`：构建并离线检查。
- `bash check_loopback.sh`：可选 Linux 回环联测，短时启动并回收测试进程。
- Windows：`.\启动研究平台.ps1` 输出全容器计划；用 `-Mode process` 输出全进程计划；`-BuildOnly` 只编译和检查。镜像配方在 `Dockerfile`。

## English

A Linux C bench for civil fixed-wing avionics. A synthetic flight-control loop links pilot input, actuation, aircraft response, sensor feedback, and the primary flight display. Navigation, propulsion and airframe indications share the same data path. The optional Genann weather workload is a side branch. Portable sample validation and static routing are separate from Linux transport.

- `bash build.sh && bash check_offline.sh`: build and validate without launching services.
- `bash check_loopback.sh`: optional short-lived Linux loopback integration test.
- On Windows, `.\启动研究平台.ps1` prints the container plan, `-Mode process` selects the all-process plan, and `-BuildOnly` compiles and checks. See `Dockerfile` for the image recipe.
