# Civil avionics weather lab / 民用航电气象台架

## 中文

用合成数据演示机载气象显示的数据流。默认扩展布局计划为 5 个 Linux 容器和 2 个普通 Linux 进程，采集侧暂由 Linux 进程代替。应用、通信和小模型均为 C。小模型用 [Genann](https://github.com/codeplea/genann) 读取气象摘要和高度上下文，计算合成回波网格的候选比例；结果只供台架观察。

- `core/`：合成数据和服务逻辑；`src/`：Linux 运行端和小模型；`config/`：服务分配；`third_party/`：上游来源和许可。
- Linux（GCC、Bash）：`bash build.sh && bash check_offline.sh`。
- Windows（Python、WSL）：`.\启动研究平台.ps1` 打印计划；加 `-BuildOnly` 编译并做离线检查。
- `Dockerfile` 提供 Linux 应用的容器镜像配方。以上命令只构建或检查，不启动应用和容器。

## English

A synthetic civil avionics weather display lab. The default expanded plan uses five Linux containers and two native Linux processes. A Linux process stands in for the acquisition side. The applications, transport, and small model are in C. [Genann](https://github.com/codeplea/genann) reads the simulated weather summary and altitude context to score a synthetic echo grid. The score is for bench observation.

- `core/`: fixtures and services; `src/`: Linux applications and model; `config/`: allocation; `third_party/`: upstream sources and license.
- Linux (GCC and Bash): `bash build.sh && bash check_offline.sh`.
- Windows (Python and WSL): `.\启动研究平台.ps1` prints the plan; add `-BuildOnly` to compile and check.
- The `Dockerfile` is the image recipe for the Linux worker. These commands do not launch applications or containers.
