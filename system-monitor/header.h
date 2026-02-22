#ifndef header_H
#define header_H

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <dirent.h>
#include <vector>
#include <iostream>
#include <cmath>
#include <fstream>
#include <unistd.h>
#include <limits.h>
#include <cpuid.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <sys/select.h>
#include <fcntl.h>
#include <ctime>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <string>
#include <cstring>

using namespace std;

// ── Structs ──────────────────────────────────────────────────────────────────

struct CPUStats {
    long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guestNice;
};

struct ProcessInfo {
    int pid;
    string name;
    char state;
    long long vsize, rss, utime, stime;
    float cpuUsage;
    float memUsage;
};
typedef ProcessInfo Proc;

struct IP4 {
    char *name;
    char addressBuffer[INET_ADDRSTRLEN];
};

struct Networks {
    vector<IP4> ip4s;
};

struct RX {
    long long bytes, packets, errs, drop, fifo, frame, compressed, multicast;
};

struct TX {
    long long bytes, packets, errs, drop, fifo, colls, carrier, compressed;
};

struct MemInfo {
    long long totalRam, freeRam, usedRam;
    long long totalSwap, freeSwap, usedSwap;
};

struct DiskInfo {
    unsigned long long total, used, free;
};

struct FanInfo {
    bool enabled, active;
    int speed, level;
    bool simulated; // New: track source
};

struct ProcessCounts {
    int running, sleeping, uninterruptible, zombie, stopped, total;
};

struct NetworkInterface {
    string name, ipv4;
    RX rx;
    TX tx;
};

// ── Function Declarations ────────────────────────────────────────────────────

// system.cpp
string CPUinfo();
const char *getOsName();
float getCPUUsage();
string getHostname();
string getUsername();
float getTemperature();
bool isThermalSimulated(); // New
FanInfo getFanInfo();
long long getUptime();
ProcessCounts getProcessCounts();
bool isVirtualMachine();
string getEnvironmentInfo();
string getHostIP();

// mem.cpp
MemInfo getMemInfo();
DiskInfo getDiskInfo();
vector<ProcessInfo> getProcessList();

// network.cpp
vector<NetworkInterface> getNetworkInterfaces();
string formatBytes(long long bytes);

#endif
