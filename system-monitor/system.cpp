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
    // Hardcoded for testing since user confirmed this is the host IP
    return "192.168.176.1";
}

// Attempts to read JSON from LHM web server: http://host:8085/data.json
static string fetchFromHost()
{
    static double lastAttemptT = 0;
    static double lastFailureT = 0;
    static string cachedData = ""; 
    double now = (double)time(NULL);

    // Cooldown on failure (2 seconds for debugging)
    if (now - lastFailureT < 2.0) return cachedData;
    if (now - lastAttemptT < 1.0) return cachedData;
    lastAttemptT = now;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8085);
    string host = getHostIP();
    
    // Convert IP correctly
    if (inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr) <= 0) {
        printf("[Bridge] Invalid IP: %s\n", host.c_str());
        close(sock); return "";
    }

    printf("[Bridge] Connecting to %s:8085...\n", host.c_str());

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    int res = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (res < 0 && errno == EINPROGRESS) {
        fd_set set; 
        struct timeval tv = {2, 0}; // 2s timeout
        FD_ZERO(&set); 
        FD_SET(sock, &set);
        int s_res = select(sock + 1, NULL, &set, NULL, &tv);
        if (s_res > 0) {
            int err; socklen_t len = sizeof(err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
            if (err == 0) res = 0;
            else errno = err;
        } else if (s_res == 0) {
            errno = ETIMEDOUT;
        }
    }

    if (res < 0) {
        lastFailureT = now;
        printf("[Bridge] Connection failed (Error %d: %s)\n", errno, strerror(errno));
        close(sock); return cachedData;
    }

    // Connected!
    printf("[Bridge] SUCCESS! Connected. Requesting data...\n");
    string req = "GET /data.json HTTP/1.1\r\nHost: " + host + ":8085\r\nConnection: close\r\n\r\n";
    send(sock, req.c_str(), req.length(), 0);

    fcntl(sock, F_SETFL, flags);
    struct timeval tv_r = {1, 0}; 
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv_r, sizeof tv_r);

    string response;
    char buf[16384]; 
    int bytes;
    while ((bytes = read(sock, buf, sizeof(buf)-1)) > 0) {
        buf[bytes] = 0;
        response += buf;
        if (response.length() > 512 * 1024) break;
    }
    close(sock);

    if (response.find("{") == string::npos) return cachedData;

    float t = -1; int f = -1;
    
    // Improved Helper: Finds a sensor by name AND type
    auto getValByNameAndType = [&](string name, string type) {
        size_t searchPos = 0;
        while ((searchPos = response.find("\"Text\":\"" + name + "\"", searchPos)) != string::npos) {
            // Check if "Type":"type" is within 200 chars of this sensor
            size_t endOfObj = response.find("}", searchPos);
            size_t typePos = response.find("\"Type\":\"" + type + "\"", searchPos);
            
            if (typePos != string::npos && (endOfObj == string::npos || typePos < endOfObj + 50)) {
                size_t valPos = response.find("\"Value\":\"", searchPos);
                if (valPos != string::npos && valPos < typePos + 100) {
                    size_t start = valPos + 9;
                    size_t end = response.find("\"", start);
                    return response.substr(start, end - start);
                }
            }
            searchPos += 10;
        }
        return string("");
    };

    // Fallback: Finds the FIRST sensor of a specific type
    auto getFirstValByType = [&](string type) {
        size_t typePos = response.find("\"Type\":\"" + type + "\"");
        if (typePos == string::npos) return string("");
        
        // Value is usually just before Type in the JSON string
        size_t startSearch = (typePos > 200) ? typePos - 200 : 0;
        size_t valPos = response.find("\"Value\":\"", startSearch);
        if (valPos != string::npos && valPos < typePos + 100) {
            size_t start = valPos + 9;
            size_t end = response.find("\"", start);
            return response.substr(start, end - start);
        }
        return string("");
    };

    string tStr = getValByNameAndType("CPU Package", "Temperature");
    if (tStr.empty()) tStr = getValByNameAndType("CPU Total", "Temperature");
    if (tStr.empty()) tStr = getFirstValByType("Temperature");

    if (!tStr.empty()) {
        size_t s = tStr.find_first_of("0123456789.-");
        if (s != string::npos) t = atof(tStr.substr(s).c_str());
    }

    string fStr = getValByNameAndType("Fan #1", "Fan");
    if (fStr.empty()) fStr = getValByNameAndType("Fan", "Fan");
    if (fStr.empty()) fStr = getFirstValByType("Fan");

    if (!fStr.empty()) {
        size_t s = fStr.find_first_of("0123456789");
        if (s != string::npos) f = atoi(fStr.substr(s).c_str());
    }

    if (t > 0 || f >= 0) {
        char out[64];
        snprintf(out, sizeof(out), "%.1f:%d:1", (t > 0 ? t : 0.0f), (f >= 0 ? f : 0));
        cachedData = out;
        printf("[Bridge] Match Found! Temp=%.1f C, Fan=%d RPM\n", t, f);
    } else {
        printf("[Bridge] No Temperature/Fan sensors found in JSON.\n");
    }
    return cachedData;
}

static bool thermalIsSim = false;

bool isThermalSimulated() { return thermalIsSim; }

float getTemperature()
{
    // 1. Try Bridge first if in VM
    if (isVirtualMachine()) {
        string data = fetchFromHost();
        // Protocol: temp:fan:is_real
        // Example: "45.5:2100:1"
        size_t c1 = data.find(':');
        size_t c2 = data.rfind(':');
        if (c1 != string::npos && c2 != string::npos && c1 != c2) {
            float t = atof(data.substr(0, c1).c_str());
            int realFlag = atoi(data.substr(c2 + 1).c_str());
            if (t > 0) {
                thermalIsSim = (realFlag == 0);
                return t;
            }
        }
    } else {
        static bool logged = false;
        if (!logged) { printf("[System] Not in VM mode, bridge inactive.\n"); logged = true; }
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

    if (temp > 0) {
        thermalIsSim = false;
        return temp;
    }

    // 3. Simulation Fallback
    thermalIsSim = true;
    static float lastSimTemp = 35.0f;
    float target = 35.0f + (getCPUUsage() * 0.4f);
    lastSimTemp = lastSimTemp * 0.9f + target * 0.1f;
    return lastSimTemp;
}

FanInfo getFanInfo()
{
    FanInfo info = {false, false, 0, 0, false};

    // 1. Try Bridge first
    if (isVirtualMachine()) {
        string data = fetchFromHost();
        size_t c1 = data.find(':');
        size_t c2 = data.rfind(':');
        if (c1 != string::npos && c2 != string::npos && c1 != c2) {
            info.speed     = atoi(data.substr(c1 + 1, c2 - c1 - 1).c_str());
            int realFlag   = atoi(data.substr(c2 + 1).c_str());
            if (info.speed > 0 || realFlag == 1) {
                info.enabled   = true;
                info.active    = (info.speed > 0);
                info.level     = (info.speed < 1200) ? 1 : (info.speed < 2000) ? 2 : (info.speed < 3000) ? 3 : 4;
                info.simulated = (realFlag == 0);
                return info;
            }
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
                    info.simulated = false;
                    closedir(dir); return info;
                }
            }
        }
        closedir(dir);
    }

    // 3. Simulation Fallback
    float cpu = getCPUUsage();
    info.enabled   = true;
    info.active    = (cpu > 2.0f);
    info.speed     = info.active ? (int)(800 + cpu * 25.0f) : 0;
    info.level     = (info.speed < 1200) ? 1 : (info.speed < 2000) ? 2 : (info.speed < 3000) ? 3 : 4;
    info.simulated = true;
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
