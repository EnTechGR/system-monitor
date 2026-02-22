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

static float cachedCPU = 0.0f;
static double lastCPUUpdate = 0.0f;

float getCPUUsage()
{
    double now = (double)time(NULL); // simple precision is enough for this
    if (now - lastCPUUpdate < 0.2) return cachedCPU; // don't poll too fast

    static long long prevIdle = 0, prevTotal = 0;
    ifstream f("/proc/stat");
    if (!f.is_open()) return cachedCPU;
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
    
    if (dTotal > 0) {
        cachedCPU = (float)(dTotal - dIdle) / (float)dTotal * 100.0f;
        prevTotal = total;
        prevIdle  = idleAll;
        lastCPUUpdate = now;
    }
    return cachedCPU;
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

bool isVirtualMachine()
{
    ifstream f("/proc/version");
    if (f.is_open()) {
        string line; getline(f, line);
        transform(line.begin(), line.end(), line.begin(), ::tolower);
        if (line.find("microsoft") != string::npos || line.find("wsl") != string::npos) return true;
    }
    ifstream f2("/proc/sys/kernel/osrelease");
    if (f2.is_open()) {
        string line; getline(f2, line);
        transform(line.begin(), line.end(), line.begin(), ::tolower);
        if (line.find("microsoft") != string::npos || line.find("wsl") != string::npos) return true;
    }
    return false;
}

string getEnvironmentInfo()
{
    if (isVirtualMachine()) return "WSL2 Virtual Machine (Virtualized)";
    return "Linux Native (Hardware Access)";
}

string getHostIP()
{
    ifstream f("/etc/resolv.conf");
    string line;
    while (getline(f, line)) {
        if (line.find("nameserver") != string::npos) {
            size_t pos = line.find_first_of("0123456789");
            if (pos != string::npos) return line.substr(pos);
        }
    }
    return "127.0.0.1";
}

// Attempts to read string from host:8085. Format expected: "temp:fan_speed"
// Example: "45.5:2100"
static string fetchFromHost()
{
    static double lastAttemptT = 0;
    static string lastResult = "";
    double now = (double)time(NULL);
    if (now - lastAttemptT < 1.0) return lastResult; // limit frequency
    lastAttemptT = now;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";

    struct timeval tv;
    tv.tv_sec = 0; tv.tv_usec = 50000; // 50ms timeout - don't hang the UI
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8085);
    string host = getHostIP();
    if (inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr) <= 0) {
        close(sock); return "";
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock); return "";
    }

    char buffer[128] = {0};
    int r = read(sock, buffer, 127);
    close(sock);
    if (r > 0) {
        lastResult = string(buffer);
        return lastResult;
    }
    return "";
}

float getTemperature()
{
    // 1. Try Bridge first if in VM
    if (isVirtualMachine()) {
        string data = fetchFromHost();
        size_t colon = data.find(':');
        if (colon != string::npos) return atof(data.substr(0, colon).c_str());
    }

    float temp = -1.0f;
    // 2. Try real hardware paths
    {
        ifstream f("/proc/acpi/ibm/thermal");
        if (f.is_open()) {
            string label; float t = -128;
            f >> label >> t;
            if (t > -128) temp = t;
        }
    }
    if (temp < 0) {
        for (int i = 0; i < 10; i++) {
            ifstream f("/sys/class/thermal/thermal_zone" + to_string(i) + "/temp");
            if (f.is_open()) { float t; f >> t; temp = t / 1000.0f; break; }
        }
    }

    // 3. Simulation Fallback
    if (temp < 0 || temp == 0) {
        static float lastSimTemp = 35.0f;
        float target = 35.0f + (getCPUUsage() * 0.4f);
        lastSimTemp = lastSimTemp * 0.9f + target * 0.1f;
        return lastSimTemp;
    }
    return temp;
}

FanInfo getFanInfo()
{
    FanInfo info = {false, false, 0, 0};

    // 1. Try Bridge first
    if (isVirtualMachine()) {
        string data = fetchFromHost();
        size_t colon = data.find(':');
        if (colon != string::npos) {
            info.speed   = atoi(data.substr(colon+1).c_str());
            info.enabled = true;
            info.active  = (info.speed > 0);
            info.level   = (info.speed < 1200) ? 1 : (info.speed < 2000) ? 2 : (info.speed < 3000) ? 3 : 4;
            return info;
        }
    }

    // 2. Try real hardware paths
    DIR *dir = opendir("/sys/class/hwmon/");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            string base = string("/sys/class/hwmon/") + entry->d_name + "/";
            for (int i = 1; i < 5; i++) {
                ifstream fi(base + "fan" + to_string(i) + "_input");
                if (fi.is_open()) {
                    fi >> info.speed;
                    info.enabled = true; info.active = (info.speed > 0);
                    info.level = (info.speed < 1000) ? 1 : 2;
                    closedir(dir); return info;
                }
            }
        }
        closedir(dir);
    }

    // 3. Simulation Fallback
    if (!info.enabled) {
        float cpu = getCPUUsage();
        info.enabled = true;
        info.active  = (cpu > 2.0f);
        info.speed   = info.active ? (int)(800 + cpu * 25.0f) : 0;
        info.level   = (info.speed < 1200) ? 1 : (info.speed < 2000) ? 2 : (info.speed < 3000) ? 3 : 4;
    }
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
