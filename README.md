# Civil fixed-wing avionics lab / 民用固定翼航电台架

## 中文

63 个 C 服务覆盖合成飞控、采集、导航、气象和机载系统。中文座舱提供主飞行、航路导航和系统三页。Linux 台架使用 Cyclone DDS，Python 提供本地预览网页。五板目标部署包含三块天脉 3 容器版与两块 Linux 板。[五板架构](docs/五板架构.md) · [机载系统调用关系](docs/机载系统调用关系.md) · [单板 RTOS DDS](docs/单板RTOS_DDS.md)

- 容器入口 `docker build -t ima-avionics-lab:local .`，再运行 `bash tools/preview_container.sh 60`。用浏览器打开命令打印的地址，按 1、2、3 切页。
- 离线检查 `bash build.sh && bash check_offline.sh`。Ubuntu 22.04 需要 `gcc libc6-dev cyclonedds-dev cyclonedds-tools`。
- Windows 用户可运行 `启动研究平台.ps1` 导出规划，或使用 `-BuildOnly` 在 WSL 构建与检查。

## English

The bench has 63 C services covering synthetic flight control, acquisition, navigation, weather and aircraft systems. The three display pages show primary flight data, navigation and aircraft systems. Linux uses Cyclone DDS and a Python preview server. [Five-board architecture](docs/五板架构.md) · [System dependencies](docs/机载系统调用关系.md) · [Board-local RTOS DDS](docs/单板RTOS_DDS.md)

- Build with `docker build -t ima-avionics-lab:local .` and run `bash tools/preview_container.sh 60`. Open the printed URL and use keys 1, 2 and 3 to switch pages.
- For offline checks, run `bash build.sh && bash check_offline.sh`. Ubuntu 22.04 requires `gcc libc6-dev cyclonedds-dev cyclonedds-tools`.
- Use the Windows PowerShell launcher to export the plan. Use `-BuildOnly` for a WSL build and offline checks.
