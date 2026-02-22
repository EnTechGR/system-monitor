# Hybrid System Monitor for Linux & WSL

A high-performance, low-overhead system monitoring tool built with **C++**, **Dear ImGui**, and **SDL2**. This monitor is uniquely designed to provide deep system telemetry for both native Linux environments and Windows Subsystem for Linux (WSL).

## 🚀 Key Features

- **Hybrid WSL Support**: Automatically detects WSL/WSL2 environments and bridges hardware data (GPU, Battery, SSD Health) from the Windows host using [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor).
- **Extreme Performance Optimization**: 
  - **Low-Frequency Mode**: Capped at 20 FPS to minimize background CPU and Power consumption.
  - **UI Clipping**: Only renders processes visible on your screen, handling hundreds of tasks with negligible lead.
  - **Single-Pass Scanning**: Consolidates multiple system-wide `/proc` scans into a single efficient operation.
- **Advanced Telemetry**:
  - **CPU**: Detailed load averages, core frequencies, voltages, and package power.
  - **GPU**: Load, temperature, and shared memory tracking (supports Intel/NVIDIA/AMD).
  - **Storage**: SSD Life percentage, TBW tracking, and temperatures.
  - **Battery**: Real-time wear level, charging status, voltage, and discharge rate.
  - **Kernel Stats**: entropy levels, system interrupts, and context switches.
- **Process Management**: Filterable process list with CPU/Mem usage tracking and memory-leak protection for closed tasks.
- **Networking**: Real-time ingress/egress monitoring across all interfaces.

## 🛠️ Prerequisites

### For Linux / WSL (Build & Run)
- `libsdl2-dev` (Essential for UI and input)
- `g++` (C++11 or later)
- `pkg-config`

### For WSL Host Bridge (Optional hardware metrics)
- [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor) running on the Windows host.
- **Remote Web Server** enabled in LHM (Options -> Remote Web Server -> Run).

## 📦 Installation & Building

1. **Install Dependencies** (Ubuntu Example):
   ```bash
   sudo apt-get update
   sudo apt-get install libsdl2-dev g++ make
   ```

2. **Clone and Build**:
   ```bash
   git clone <repository-url>
   cd system-monitor/system-monitor
   make
   ```

3. **Run**:
   ```bash
   ./monitor
   ```

## 🌐 Setting up the WSL Bridge

To see GPU, SSD, and Battery info in WSL:
1. Download [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor) on Windows.
2. Go to **Options** -> **Remote Web Server** -> **Run**.
3. Ensure the server is listening (default: port 8085).
4. Run the `./monitor` app in WSL. It will automatically detect the host IP and start bridging the data.

## ⚖️ Performance Notes
This application is designed to be "invisible" to your system's resources:
- **CPU Usage**: Typically **<5%** on modern systems.
- **Memory Footprint**: Stable at **~1.5%** of system RAM.
- **Refresh Rate**: Graphs and heavy stats update every 2 seconds, while the UI responds at 20 FPS.

## 📄 License
This project is licensed under the MIT License. It includes [Dear ImGui](https://github.com/ocornut/imgui) (MIT) and [gl3w](https://github.com/skaslev/gl3w) (Unlicense).
