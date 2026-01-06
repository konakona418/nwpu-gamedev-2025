# NWPU GameDev 2025 Autumn

<div align="center">
  <h3>With Moe Graphics Engine</h3>
  <img src="./img/moe-graphics-logo.png" width="480" />
</div>

## Basic Info

西北工业大学 软件学院

软件开发基础能力训练 - 游戏开发方向

本项目实现了一个类似 Counter-Strike 的第一人称射击游戏。

## Architecture

基于 Vulkan 1.3 开发的图形库，支持 2D、3D 渲染。使用 Slang 作为着色器编程语言。

基于 JoltPhysics 的物理系统。

基于 ENet 和 FlatBuffers 的网络通信。

基于 OpenAL 的音频。

## Deploy Requirements

要求构建此项目的设备必须具有以下依赖：

- CMake 3.10
- Git
- Python 3
- Slang 相关工具
- FlatBuffers 相关工具

本项目仅保证对 Windows 平台下 MinGW GCC 和 Clang 的支持，部分特性可能对 MSVC 及其他平台不适用。

本项目只适用于支持 Vulkan 1.3 及更新版本，且支持大部分关键 Vulkan 特性的设备。

使用 `USE_MIMALLOC` CMake 构建选项可以部分开启 mimalloc 支持，但此支持未经过严格测试，可能导致运行不稳定。

## External Links

[服务端](https://github.com/Muyunaaaa/nwpu-gamedev-2025-server)

## Dev Team

- 冯于洋
- 李佳睿
- 李梓萌
- 滕艺斐
- 吴焕石
