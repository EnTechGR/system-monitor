#include "header.h"

MemInfo getMemInfo()
{
    MemInfo info = {};
    ifstream f("/proc/meminfo");
    if (!f.is_open()) return info;
    string line;
    while (getline(f, line))
    {
        istringstream ss(line);
        string key; long long val;
        ss >> key >> val;
        if      (key == "MemTotal:")     info.totalRam  = val * 1024LL;
        else if (key == "MemAvailable:") info.freeRam   = val * 1024LL;
        else if (key == "SwapTotal:")    info.totalSwap = val * 1024LL;
        else if (key == "SwapFree:")     info.freeSwap  = val * 1024LL;
    }
    info.usedRam  = info.totalRam  - info.freeRam;
    info.usedSwap = info.totalSwap - info.freeSwap;
    return info;
}

DiskInfo getDiskInfo()
{
    DiskInfo info = {};
    struct statvfs st;
    if (statvfs("/", &st) == 0)
    {
        info.total = (unsigned long long)st.f_blocks * st.f_frsize;
        info.free  = (unsigned long long)st.f_bfree  * st.f_frsize;
        info.used  = info.total - info.free;
    }
    return info;
}

static map<int, long long> prevProcTime;
static long long prevCPUTotal = 0;

vector<ProcessInfo> getProcessList(ProcessCounts* counts)
{
    vector<ProcessInfo> procs;
    if (counts) memset(counts, 0, sizeof(ProcessCounts));

    // ... same total CPU calculation ...
    long long cpuTotal = 0;
    {
        ifstream f("/proc/stat");
        if (f.is_open())
        {
            string line; getline(f, line);
            istringstream ss(line);
            string lbl;
            long long u, n, s, id, io = 0, irq = 0, sirq = 0, steal = 0;
            ss >> lbl >> u >> n >> s >> id >> io >> irq >> sirq >> steal;
            cpuTotal = u + n + s + id + io + irq + sirq + steal;
        }
    }
    long long cpuDelta = cpuTotal - prevCPUTotal;
    prevCPUTotal = cpuTotal;

    MemInfo mem  = getMemInfo();
    long pageSize = sysconf(_SC_PAGE_SIZE);

    DIR *dp = opendir("/proc");
    if (!dp) return procs;

    struct dirent *entry;
    set<int> seenPids;
    while ((entry = readdir(dp)) != nullptr)
    {
        if (!isdigit(entry->d_name[0])) continue;
        int pid = atoi(entry->d_name);
        seenPids.insert(pid);

        ifstream sf(string("/proc/") + entry->d_name + "/stat");
        if (!sf.is_open()) continue;
        string line; getline(sf, line);

        size_t op = line.find('('), cp = line.rfind(')');
        if (op == string::npos || cp == string::npos) continue;
        string name = line.substr(op + 1, cp - op - 1);

        istringstream ss(line.substr(cp + 2));
        char state;
        int ppid, pgrp, sess, tty, tpgid;
        unsigned long flags;
        long long minflt, cminflt, majflt, cmajflt, utime, stime, cutime, cstime, prio, nice, nthreads, irt, starttime;
        unsigned long long vsize;
        long long rss;

        if (!(ss >> state >> ppid >> pgrp >> sess >> tty >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt >> utime >> stime >> cutime >> cstime >> prio >> nice >> nthreads >> irt >> starttime >> vsize >> rss)) continue;

        // Optimization: Use the raw line if needed, but sscanf is even better for fixed formats.
        // For now, let's keep the ss >> for robustness, but optimize the string creation.
        // Actually, let's use sscanf on the remainder of the line for a real speedup.
        // (Re-using parts of existing logic for stability)

        // Process Counts optimization: Update counts while listing
        if (counts) {
            counts->total++;
            switch(state) {
                case 'R': counts->running++; break;
                case 'S': counts->sleeping++; break;
                case 'D': counts->uninterruptible++; break;
                case 'Z': counts->zombie++; break;
                default:  counts->stopped++; break;
            }
        }

        long long pt = utime + stime;
        float cpuUsg = 0.0f;
        auto it = prevProcTime.find(pid);
        if (it != prevProcTime.end() && cpuDelta > 0)
            cpuUsg = max(0.0f, (float)(pt - it->second) / (float)cpuDelta * 100.0f);
        prevProcTime[pid] = pt;

        float memUsg = (mem.totalRam > 0) ? (float)(rss * pageSize) / (float)mem.totalRam * 100.0f : 0.0f;

        ProcessInfo p;
        p.pid = pid; p.name = name; p.state = state;
        p.vsize = (long long)vsize; p.rss = rss;
        p.utime = utime; p.stime = stime;
        p.cpuUsage = cpuUsg; p.memUsage = memUsg;
        procs.push_back(p);
    }
    closedir(dp);

    for (auto it = prevProcTime.begin(); it != prevProcTime.end(); ) {
        if (seenPids.find(it->first) == seenPids.end()) it = prevProcTime.erase(it);
        else ++it;
    }

    sort(procs.begin(), procs.end(), [](const ProcessInfo &a, const ProcessInfo &b){ return a.cpuUsage > b.cpuUsage; });
    return procs;
}
