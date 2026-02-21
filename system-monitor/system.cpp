#include "header.h"

string CPUinfo()
{
    char CPUBrandString[0x40];
    unsigned int CPUInfo[4] = {0, 0, 0, 0};
    __cpuid(0x80000000, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
    unsigned int nExIds = CPUInfo[0];
    memset(CPUBrandString, 0, sizeof(CPUBrandString));
    for (unsigned int i = 0x80000000; i <= nExIds; ++i)
    {
        __cpuid(i, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
        if (i == 0x80000002) memcpy(CPUBrandString,      CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000003) memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000004) memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
    }
    return string(CPUBrandString);
}

const char *getOsName()
{
#ifdef _WIN32
    return "Windows 32-bit";
#elif _WIN64
    return "Windows 64-bit";
#elif __APPLE__ || __MACH__
    return "Mac OSX";
#elif __linux__
    return "Linux";
#elif __FreeBSD__
    return "FreeBSD";
#elif __unix || __unix__
    return "Unix";
#else
    return "Other";
#endif
}

float getCPUUsage()
{
    static long long prevIdle = 0, prevTotal = 0;
    ifstream f("/proc/stat");
    if (!f.is_open()) return 0.0f;
    string line;
    getline(f, line);
    istringstream ss(line);
    string cpu;
    long long user, nice, sys, idle, iowait = 0, irq = 0, softirq = 0, steal = 0;
    ss >> cpu >> user >> nice >> sys >> idle >> iowait >> irq >> softirq >> steal;
    long long idleAll = idle + iowait;
    long long total   = user + nice + sys + idle + iowait + irq + softirq + steal;
    long long dTotal  = total - prevTotal;
    long long dIdle   = idleAll - prevIdle;
    prevTotal = total;
    prevIdle  = idleAll;
    if (dTotal == 0) return 0.0f;
    return (float)(dTotal - dIdle) / (float)dTotal * 100.0f;
}

string getHostname()
{
    char buf[HOST_NAME_MAX];
    if (gethostname(buf, HOST_NAME_MAX) == 0) return string(buf);
    return "unknown";
}

string getUsername()
{
    const char *u = getenv("USER");
    if (u) return string(u);
    u = getenv("LOGNAME");
    if (u) return string(u);
    return "unknown";
}

float getTemperature()
{
    for (int i = 0; i < 6; i++)
    {
        ifstream f("/sys/class/thermal/thermal_zone" + to_string(i) + "/temp");
        if (f.is_open())
        {
            float t; f >> t;
            return t / 1000.0f;
        }
    }
    return -1.0f;
}

FanInfo getFanInfo()
{
    FanInfo info = {false, false, 0, 0};
    DIR *dir = opendir("/sys/class/hwmon/");
    if (!dir) return info;
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (entry->d_name[0] == '.') continue;
        string base = string("/sys/class/hwmon/") + entry->d_name + "/";
        ifstream fi(base + "fan1_input");
        if (fi.is_open())
        {
            fi >> info.speed;
            info.enabled = true;
            info.active  = (info.speed > 0);
            info.level   = (info.speed < 1000) ? 1 : (info.speed < 2000) ? 2 : (info.speed < 3000) ? 3 : 4;
            break;
        }
    }
    closedir(dir);
    return info;
}

long long getUptime()
{
    ifstream f("/proc/uptime");
    double t = 0;
    f >> t;
    return (long long)t;
}

ProcessCounts getProcessCounts()
{
    ProcessCounts c = {0, 0, 0, 0, 0, 0};
    DIR *dir = opendir("/proc");
    if (!dir) return c;
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (!isdigit(entry->d_name[0])) continue;
        ifstream f(string("/proc/") + entry->d_name + "/stat");
        if (!f.is_open()) continue;
        string line; getline(f, line);
        size_t cp = line.rfind(')');
        if (cp == string::npos || cp + 2 >= line.size()) continue;
        char state = line[cp + 2];
        switch (state)
        {
            case 'R': c.running++;        break;
            case 'S': c.sleeping++;       break;
            case 'D': c.uninterruptible++;break;
            case 'Z': c.zombie++;         break;
            case 'T': case 't': c.stopped++; break;
        }
        c.total++;
    }
    closedir(dir);
    return c;
}
