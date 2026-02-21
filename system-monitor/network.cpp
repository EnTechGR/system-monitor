#include "header.h"

string formatBytes(long long bytes)
{
    if (bytes < 0) bytes = 0;
    char buf[64];
    if (bytes < 1024LL * 1024)
        snprintf(buf, sizeof(buf), "%.2f KB", (double)bytes / 1024.0);
    else if (bytes < 1024LL * 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.2f MB", (double)bytes / (1024.0 * 1024));
    else
        snprintf(buf, sizeof(buf), "%.2f GB", (double)bytes / (1024.0 * 1024 * 1024));
    return string(buf);
}

vector<NetworkInterface> getNetworkInterfaces()
{
    map<string, NetworkInterface> ifMap;

    // IPv4 addresses via getifaddrs
    struct ifaddrs *ifaddr;
    if (getifaddrs(&ifaddr) == 0)
    {
        for (struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next)
        {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            char addr[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
                      addr, INET_ADDRSTRLEN);
            ifMap[ifa->ifa_name].name  = ifa->ifa_name;
            ifMap[ifa->ifa_name].ipv4  = addr;
        }
        freeifaddrs(ifaddr);
    }

    // RX/TX stats from /proc/net/dev
    ifstream f("/proc/net/dev");
    if (f.is_open())
    {
        string line;
        getline(f, line); // header 1
        getline(f, line); // header 2
        while (getline(f, line))
        {
            size_t col = line.find(':');
            if (col == string::npos) continue;
            string name = line.substr(0, col);
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t") + 1);

            istringstream ss(line.substr(col + 1));
            RX rx = {}; TX tx = {};
            ss >> rx.bytes >> rx.packets >> rx.errs >> rx.drop
               >> rx.fifo  >> rx.frame   >> rx.compressed >> rx.multicast;
            ss >> tx.bytes >> tx.packets >> tx.errs >> tx.drop
               >> tx.fifo  >> tx.colls   >> tx.carrier    >> tx.compressed;

            ifMap[name].name = name;
            ifMap[name].rx   = rx;
            ifMap[name].tx   = tx;
        }
    }

    vector<NetworkInterface> result;
    for (auto &[k, v] : ifMap) result.push_back(v);
    sort(result.begin(), result.end(), [](const NetworkInterface &a, const NetworkInterface &b){
        if (a.name == "lo") return true;
        if (b.name == "lo") return false;
        return a.name < b.name;
    });
    return result;
}
