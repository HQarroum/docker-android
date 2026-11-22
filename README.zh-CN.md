# docker-android 中文说明

一个可定制的 Docker Android 模拟器镜像。容器内运行 Android Emulator，并通过 ADB、`scrcpy` 或 noVNC 对外访问。

English version: [README.md](README.md)

## 功能

- 在容器内运行 Android 模拟器
- 支持 KVM 硬件虚拟化
- 支持按构建参数选择 Android API、镜像类型和架构
- 暴露 ADB 端口，宿主机可直接连接
- 支持宿主机通过 `scrcpy` 控制模拟器
- 支持通过 noVNC 在浏览器中查看和操作模拟器

## 使用方式

默认构建会安装 Android SDK、platform-tools、emulator 和目标 system image。

使用普通版本：

```bash
docker compose up --build android-emulator
```

使用 GPU 版本：

```bash
docker compose up --build android-emulator-cuda
```

使用 GPU + Play Store 版本：

```bash
docker compose up --build android-emulator-cuda-store
```

如果只构建镜像：

```bash
docker build -t android-emulator .
```

## ADB Key

如果使用 `google_apis_playstore` 镜像，建议让客户端和模拟器使用同一组 ADB key。

生成方式：

```bash
adb keygen adbkey
```

把生成的 `adbkey` 和 `adbkey.pub` 放到 `./keys/` 目录下。

## 运行容器

示例：

```bash
docker run -it --rm --device /dev/kvm -p 5555:5555 android-emulator
```

如果需要持久化 AVD 数据：

```bash
docker run -it --rm --device /dev/kvm -p 5555:5555 -v ~/android_avd:/data android-emulator
```

当前仓库默认使用 compose，并额外暴露 noVNC 端口 `6080`。

## 通过 ADB 访问

容器启动后，内部 ADB server 会自动启动。模拟器启动完成后，可在宿主机执行：

```bash
adb connect 127.0.0.1:5555
```

## 通过 scrcpy 访问

如果宿主机已经安装 `scrcpy`，在完成 `adb connect` 后直接执行：

```bash
scrcpy
```

这种方式适合本地开发机直接控制模拟器。

## 通过 noVNC 浏览器访问

本仓库已经在 `docker-compose.yml` 中启用了 noVNC。启动容器后，可以直接用浏览器访问容器中的模拟器窗口。

启动：

```bash
docker compose up --build android-emulator
```

浏览器访问：

```text
http://127.0.0.1:6080/vnc.html
```

说明：

- noVNC 访问不需要本机安装 `scrcpy`
- 宿主机仍然可以同时使用 `adb connect 127.0.0.1:5555`
- 如果需要调整浏览器中显示的分辨率，可修改 `DISPLAY_WIDTH`、`DISPLAY_HEIGHT`、`DISPLAY_DPI`

## 当前 compose 默认参数

当前仓库的 `docker-compose.yml` 默认包含这些配置：

- `DISABLE_ANIMATION=true`
- `DISABLE_HIDDEN_POLICY=true`
- `SKIP_AUTH=false`
- `ENABLE_NOVNC=true`
- `DISPLAY_WIDTH=1080`
- `DISPLAY_HEIGHT=1920`
- `DISPLAY_DPI=160`
- `MEMORY=16384`
- `CORES=16`

## 可配置变量

- `DISABLE_ANIMATION`
  是否关闭系统动画
- `DISABLE_HIDDEN_POLICY`
  是否关闭 hidden API policy
- `SKIP_AUTH`
  是否跳过 ADB 认证
- `ENABLE_NOVNC`
  是否启用 noVNC 浏览器访问
- `DISPLAY_WIDTH`
  noVNC 虚拟显示宽度
- `DISPLAY_HEIGHT`
  noVNC 虚拟显示高度
- `DISPLAY_DPI`
  noVNC 虚拟显示 DPI
- `MEMORY`
  模拟器内存大小
- `CORES`
  模拟器 CPU 核数
- `EXTRA_FLAGS`
  额外传给 emulator 的参数

## 自定义镜像

可以通过构建参数指定 API、镜像类型和架构。

示例：

```bash
docker build \
  --build-arg API_LEVEL=28 \
  --build-arg IMG_TYPE=google_apis_playstore \
  --build-arg ARCHITECTURE=x86 \
  --tag android-emulator .
```

## 说明

- noVNC 适合“只想通过浏览器查看和操作模拟器”的场景
- `scrcpy` 适合“本机需要更直接的桌面控制体验”的场景
- 如果追求性能，优先保证 `/dev/kvm` 可用，并尽量把 AVD 数据放在 Linux 原生文件系统而不是慢速挂载盘上
