# Neko Kernel Manager

![Status](https://img.shields.io/badge/status-in--development-yellow)

<img width="1089" height="695" alt="image" src="https://github.com/user-attachments/assets/7c38f199-b1e3-4502-8678-690b46ccb730" />

<img width="1089" height="695" alt="image" src="https://github.com/user-attachments/assets/43bb6181-78da-4fc7-a9df-0c2906bd1c42" />

<img width="1089" height="695" alt="image" src="https://github.com/user-attachments/assets/22d0b051-e896-428c-ae75-ce92afe890e3" />

<img width="1039" height="517" alt="image" src="https://github.com/user-attachments/assets/2cbab0ea-bf2b-4c74-b87e-48989f88e0d1" />

A comprehensive kernel manager for Void Linux, built with C++ (Qt6/QML) and Rust.

> **Note**: This project is currently **under active development**. Features may change, and you might encounter bugs.

## Features
- List and manage kernels from Void repositories (Stable, LTS, RT, Zen, Hardened, Mainline)
- Install and remove kernels with privilege escalation
- Download kernels directly from kernel.org (select variants and versions: stable, lts, mainline, all)
- Clone custom kernels from Git repositories
- Build custom kernels using Void templates
- Export compiled kernel packages
- Kernel configuration options:
  - LTO (Link Time Optimization)
  - Preemption models (none, voluntary, full)
  - CPU optimizations (generic, native, zen2, zen3, skylake)
  - ZFS filesystem support
  - NVIDIA driver support
- Save and load configuration profiles
- Modern Qt6 QML interface

## Build
```bash
./build.sh
# or manually:
meson setup build --buildtype=release
meson compile -C build
```

## Dependencies
- Qt6 (Core, GUI, QML, Quick, Concurrent, Widgets)
- Rust (with Serde)
- curl, git
- xbps (Void package manager)
- fmt library

## Usage
Run `./build/neko-kernel-manager` to start the GUI.

- **XBPS Kernels**: Manage official Void kernels
- **Downloads**: Download from kernel.org or Git repos
- **Build & Config**: Configure and build custom kernels
