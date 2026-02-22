#include "header.h"
#include <SDL.h>

#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD2)
#include <glad/gl.h>
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
#define GLFW_INCLUDE_NONE
#include <glbinding/Binding.h>
#include <glbinding/gl/gl.h>
using namespace gl;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
#define GLFW_INCLUDE_NONE
#include <glbinding/glbinding.h>
#include <glbinding/gl/gl.h>
using namespace gl;
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

// ── Helpers ──────────────────────────────────────────────────────────────────

static const int HIST = 512;

struct Graph {
    float  data[HIST] = {};
    int    offset = 0;
    int    count  = 0;
    bool   stopped = false;
    float  fps    = 2.0f;   // 2 samples/sec = stable 500 ms readings
    float  scale  = 100.0f;
    double lastT  = -1.0;

    void push(float v) {
        data[offset % HIST] = v;
        offset++;
        count = min(count + 1, HIST);
    }
    void draw(const char *id, const char *overlay, float height) {
        // PlotLines does NOT support -1 for auto-width; must be explicit or 0
        float w = ImGui::GetContentRegionAvail().x;
        // Need at least 2 points for PlotLines to draw a bordered box
        int cnt = (count >= 2) ? count : 2;
        int off = (offset >= HIST) ? (offset % HIST) : 0;
        ImGui::PlotLines(id, data, cnt, off, overlay, 0.0f, scale, ImVec2(w, height));
    }
    void controls(const char *stopId, const char *fpsId, const char *scaleId,
                  float sMin = 10.f, float sMax = 200.f) {
        ImGui::Checkbox(stopId, &stopped);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110);
        ImGui::SliderFloat(fpsId, &fps, 1.0f, 60.0f, "FPS: %.0f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(130);
        ImGui::SliderFloat(scaleId, &scale, sMin, sMax, "Sc: %.0f");
    }
    bool due(double now) {
        if (stopped) return false;
        if (now - lastT < 1.0 / fps) return false;
        lastT = now;
        return true;
    }
};

static void progressBar(const char *label, long long used, long long total, const char *unit = "GB")
{
    if (total <= 0) { ImGui::Text("%s: N/A", label); return; }
    float frac = (float)used / (float)total;
    double scale = 1024.0 * 1024 * 1024;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f / %.1f %s", used / scale, total / scale, unit);
    ImGui::Text("%s", label);
    ImGui::SameLine();
    ImGui::ProgressBar(frac, ImVec2(-1, 0), buf);
}

// ── System Window ─────────────────────────────────────────────────────────────

void systemWindow(const char *id, ImVec2 size, ImVec2 position)
{
    ImGui::Begin(id);
    ImGui::SetWindowSize(id, size);
    ImGui::SetWindowPos(id, position);

    static string hostname = getHostname();
    static string username = getUsername();
    static string cpuName  = CPUinfo();

    static ProcessCounts pCounts = {};
    static long long uptime = 0;
    static double lastInfoT = -5.0;

    static Graph cpuG, fanG, thermalG;
    static FanInfo fanInfo = {};
    static float cpuCur = 0, thermalCur = 0;

    // One-time: set sensible default scales per graph
    static bool graphsInit = false;
    if (!graphsInit) {
        cpuG.scale     = 10.0f;    // 0-10% range by default; user can adjust
        fanG.scale     = 2000.0f;  // 0-2000 RPM default
        thermalG.scale = 100.0f;   // 0-100 °C default
        graphsInit = true;
    }

    double now = ImGui::GetTime();

    // Refresh slow data every 2 s
    if (now - lastInfoT > 2.0) {
        uptime   = getUptime();
        lastInfoT = now;
    }

    // Update graphs
    if (cpuG.due(now)) {
        cpuCur = getCPUUsage();
        cpuG.push(cpuCur);
    }
    if (fanG.due(now)) {
        fanInfo = getFanInfo();
        fanG.push((float)fanInfo.speed);
    }
    if (thermalG.due(now)) {
        float t = getTemperature();
        thermalCur = (t < 0) ? 0 : t;
        thermalG.push(thermalCur);
    }

    // ── Info rows ───────────────────────────────────────────────────────────
    ImGui::TextColored(ImVec4(0.4f,0.8f,1.0f,1), "OS");       ImGui::SameLine(80); ImGui::Text("%s", getOsName());
    ImGui::TextColored(ImVec4(0.4f,0.8f,1.0f,1), "Env");      ImGui::SameLine(80); ImGui::Text("%s", getEnvironmentInfo().c_str());
    if (isVirtualMachine()) {
        ImGui::TextColored(ImVec4(0.4f,0.8f,1.0f,1), "Bridge");   ImGui::SameLine(80); ImGui::Text("Host IP: %s (Port 8085)", getHostIP().c_str());
    }
    ImGui::TextColored(ImVec4(0.4f,0.8f,1.0f,1), "User");     ImGui::SameLine(80); ImGui::Text("%s", username.c_str());
    ImGui::TextColored(ImVec4(0.4f,0.8f,1.0f,1), "Host");     ImGui::SameLine(80); ImGui::Text("%s", hostname.c_str());

    long long d = uptime/86400, h = (uptime%86400)/3600, m = (uptime%3600)/60, s = uptime%60;
    char uptStr[64];
    if (d) snprintf(uptStr,sizeof(uptStr),"%lld d %02lld:%02lld:%02lld",d,h,m,s);
    else   snprintf(uptStr,sizeof(uptStr),"%02lld:%02lld:%02lld",h,m,s);
    ImGui::TextColored(ImVec4(0.4f,0.8f,1.0f,1), "Uptime");   ImGui::SameLine(80); ImGui::Text("%s", uptStr);
    ImGui::TextColored(ImVec4(0.4f,0.8f,1.0f,1), "CPU");      ImGui::SameLine(80); ImGui::Text("%s", cpuName.c_str());

    ImGui::Separator();
    ImGui::TextColored(ImVec4(1,0.85f,0.3f,1),
        "Tasks: %d total  |  %d running  |  %d sleeping  |  %d uninterruptible  |  %d zombie  |  %d stopped",
        pCounts.total, pCounts.running, pCounts.sleeping,
        pCounts.uninterruptible, pCounts.zombie, pCounts.stopped);
    ImGui::Separator();

    // ── Tabs ────────────────────────────────────────────────────────────────
    if (ImGui::BeginTabBar("sysTabs"))
    {
        if (ImGui::BeginTabItem("CPU"))
        {
            // Live readout — always visible regardless of scale
            ImGui::TextColored(ImVec4(0.3f,0.9f,0.4f,1), "Current CPU: %.2f %%", cpuCur);
            ImGui::SameLine(220);
            ImGui::TextDisabled("(Ctrl+drag slider for fine control)");
            // sMin=1 so even 1% CPU fills half the graph when scale=2
            cpuG.controls("Stop##cs","##cf","##csc", 1.0f, 100.0f);
            char ov[32]; snprintf(ov, sizeof(ov), "%.1f%%", cpuCur);
            cpuG.draw("##cg", ov, 120.0f);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Fan"))
        {
            if (fanInfo.enabled) {
                ImGui::TextColored(ImVec4(0.3f,0.9f,0.4f,1), "Status: Enabled  |  Active: %s  |  Speed: %d RPM  |  Level: %d",
                    fanInfo.active ? "Yes" : "No", fanInfo.speed, fanInfo.level);
                ImGui::SameLine();
                if (fanInfo.simulated) ImGui::TextColored(ImVec4(1,0.6f,0,1), " (Simulated)");
                else                   ImGui::TextColored(ImVec4(0,1,0,1), " (Real)");
            } else {
                ImGui::TextColored(ImVec4(1,0.6f,0.2f,1), "Fan sensor not available on this system");
                ImGui::Text("Status: N/A  |  Active: No  |  Speed: 0 RPM  |  Level: 0");
            }
            ImGui::Spacing();
            fanG.controls("Stop##fs","##ff","##fsc", 1.0f, 8000.0f);
            char ov[32]; snprintf(ov, sizeof(ov), "%d RPM", fanInfo.speed);
            fanG.draw("##fg", ov, 100.0f);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Thermal"))
        {
            if (thermalCur > 0) {
                ImGui::TextColored(ImVec4(0.3f,0.9f,0.4f,1), "Current Temp: %.1f °C", thermalCur);
                ImGui::SameLine();
                if (isThermalSimulated()) ImGui::TextColored(ImVec4(1,0.6f,0,1), " (Simulated)");
                else                     ImGui::TextColored(ImVec4(0,1,0,1), " (Real)");
            }
            else
                ImGui::TextColored(ImVec4(1,0.6f,0.2f,1), "Thermal sensor not available on this system");
            thermalG.controls("Stop##ts","##tf","##tsc", 1.0f, 200.0f);
            char ov[32];
            if (thermalCur > 0) snprintf(ov, sizeof(ov), "%.1f C", thermalCur);
            else                snprintf(ov, sizeof(ov), "N/A");
            thermalG.draw("##tg", ov, 120.0f);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ── Memory & Processes Window ─────────────────────────────────────────────────

void memoryProcessesWindow(const char *id, ImVec2 size, ImVec2 position)
{
    ImGui::Begin(id);
    ImGui::SetWindowSize(id, size);
    ImGui::SetWindowPos(id, position);

    static MemInfo mem = {};
    static DiskInfo disk = {};
    static vector<ProcessInfo> procs;
    static double lastUpdateT = -5.0;
    static char filterBuf[128] = {};

    static ProcessCounts pCountsBuf = {};
    double now = ImGui::GetTime();
    if (now - lastUpdateT > 2.0) {
        mem  = getMemInfo();
        disk = getDiskInfo();
        procs = getProcessList(&pCountsBuf);
        lastUpdateT = now;
    }

    // ── Usage bars ──────────────────────────────────────────────────────────
    progressBar("RAM ", mem.usedRam,  mem.totalRam);
    progressBar("SWAP", mem.usedSwap, mem.totalSwap);

    // Disk bar (show in GB)
    if (disk.total > 0) {
        float frac = (float)disk.used / (float)disk.total;
        double g = 1024.0 * 1024 * 1024;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.1f / %.1f GB", disk.used/g, disk.total/g);
        ImGui::Text("Disk");
        ImGui::SameLine();
        ImGui::ProgressBar(frac, ImVec2(-1, 0), buf);
    }
    ImGui::Separator();

    // ── Process table ───────────────────────────────────────────────────────
    if (ImGui::BeginTabBar("memTabs"))
    {
        if (ImGui::BeginTabItem("Processes"))
        {
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##filter", filterBuf, sizeof(filterBuf),
                             ImGuiInputTextFlags_None);
            ImGui::SameLine(0, 0);
            ImGui::TextDisabled(" Filter");

            static ImGuiTableFlags tflags =
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable;

            if (ImGui::BeginTable("procs", 5, tflags,
                ImVec2(0, ImGui::GetContentRegionAvail().y)))
            {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("PID",    ImGuiTableColumnFlags_WidthFixed, 60);
                ImGui::TableSetupColumn("Name",   ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("State",  ImGuiTableColumnFlags_WidthFixed, 50);
                ImGui::TableSetupColumn("CPU %",  ImGuiTableColumnFlags_WidthFixed, 65);
                ImGui::TableSetupColumn("Mem %",  ImGuiTableColumnFlags_WidthFixed, 65);
                ImGui::TableHeadersRow();

                string flt = string(filterBuf);
                static set<int> selectedPids;

                ImGuiListClipper clipper;
                clipper.Begin(procs.size());
                while (clipper.Step())
                {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    {
                        auto &p = procs[i];
                        if (!flt.empty() && 
                            p.name.find(flt) == string::npos && 
                            to_string(p.pid).find(flt) == string::npos) continue;

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);

                        bool isSelected = selectedPids.count(p.pid) > 0;
                        char selId[32];
                        snprintf(selId, sizeof(selId), "##sel%d", p.pid);

                        if (ImGui::Selectable(selId, isSelected,
                            ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap,
                            ImVec2(0, 0)))
                        {
                            if (ImGui::GetIO().KeyCtrl)
                            {
                                if (isSelected) selectedPids.erase(p.pid);
                                else            selectedPids.insert(p.pid);
                            }
                            else
                            {
                                selectedPids.clear();
                                selectedPids.insert(p.pid);
                            }
                        }
                        ImGui::SameLine();
                        ImGui::Text("%d", p.pid);
                        ImGui::TableNextColumn(); ImGui::Text("%s", p.name.c_str());
                        ImGui::TableNextColumn(); ImGui::Text("%c", p.state);
                        ImGui::TableNextColumn(); ImGui::Text("%.2f", p.cpuUsage);
                        ImGui::TableNextColumn(); ImGui::Text("%.2f", p.memUsage);
                    }
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ── Network Window ────────────────────────────────────────────────────────────

void networkWindow(const char *id, ImVec2 size, ImVec2 position)
{
    ImGui::Begin(id);
    ImGui::SetWindowSize(id, size);
    ImGui::SetWindowPos(id, position);

    static vector<NetworkInterface> ifaces;
    static double lastNetT = -5.0;
    double now = ImGui::GetTime();
    if (now - lastNetT > 2.0) {
        ifaces    = getNetworkInterfaces();
        lastNetT  = now;
    }

    // ── IPv4 addresses ──────────────────────────────────────────────────────
    ImGui::TextColored(ImVec4(0.4f,0.8f,1.0f,1), "IPv4 Addresses:");
    ImGui::SameLine();
    for (auto &iface : ifaces)
    {
        ImGui::TextDisabled("  %s: %s",
            iface.name.c_str(),
            iface.ipv4.empty() ? "—" : iface.ipv4.c_str());
        ImGui::SameLine();
    }
    ImGui::NewLine();
    ImGui::Separator();

    // ── Tabs ────────────────────────────────────────────────────────────────
    if (ImGui::BeginTabBar("netTabs"))
    {
        // RX Table
        if (ImGui::BeginTabItem("RX"))
        {
            static ImGuiTableFlags tf =
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollX;
            if (ImGui::BeginTable("rxTable", 9, tf))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("Interface");
                ImGui::TableSetupColumn("Bytes");
                ImGui::TableSetupColumn("Packets");
                ImGui::TableSetupColumn("Errs");
                ImGui::TableSetupColumn("Drop");
                ImGui::TableSetupColumn("Fifo");
                ImGui::TableSetupColumn("Frame");
                ImGui::TableSetupColumn("Compressed");
                ImGui::TableSetupColumn("Multicast");
                ImGui::TableHeadersRow();
                for (auto &iface : ifaces)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%s",    iface.name.c_str());
                    ImGui::TableNextColumn(); ImGui::Text("%lld",  iface.rx.bytes);
                    ImGui::TableNextColumn(); ImGui::Text("%lld",  iface.rx.packets);
                    ImGui::TableNextColumn(); ImGui::Text("%lld",  iface.rx.errs);
                    ImGui::TableNextColumn(); ImGui::Text("%lld",  iface.rx.drop);
                    ImGui::TableNextColumn(); ImGui::Text("%lld",  iface.rx.fifo);
                    ImGui::TableNextColumn(); ImGui::Text("%lld",  iface.rx.frame);
                    ImGui::TableNextColumn(); ImGui::Text("%lld",  iface.rx.compressed);
                    ImGui::TableNextColumn(); ImGui::Text("%lld",  iface.rx.multicast);
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        // TX Table
        if (ImGui::BeginTabItem("TX"))
        {
            static ImGuiTableFlags tf =
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollX;
            if (ImGui::BeginTable("txTable", 9, tf))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn("Interface");
                ImGui::TableSetupColumn("Bytes");
                ImGui::TableSetupColumn("Packets");
                ImGui::TableSetupColumn("Errs");
                ImGui::TableSetupColumn("Drop");
                ImGui::TableSetupColumn("Fifo");
                ImGui::TableSetupColumn("Colls");
                ImGui::TableSetupColumn("Carrier");
                ImGui::TableSetupColumn("Compressed");
                ImGui::TableHeadersRow();
                for (auto &iface : ifaces)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%s",    iface.name.c_str());
                    ImGui::TableNextColumn(); ImGui::Text("%lld",  iface.tx.bytes);
                    ImGui::TableNextColumn(); ImGui::Text("%lld",  iface.tx.packets);
                    ImGui::TableNextColumn(); ImGui::Text("%lld",  iface.tx.errs);
                    ImGui::TableNextColumn(); ImGui::Text("%lld",  iface.tx.drop);
                    ImGui::TableNextColumn(); ImGui::Text("%lld",  iface.tx.fifo);
                    ImGui::TableNextColumn(); ImGui::Text("%lld",  iface.tx.colls);
                    ImGui::TableNextColumn(); ImGui::Text("%lld",  iface.tx.carrier);
                    ImGui::TableNextColumn(); ImGui::Text("%lld",  iface.tx.compressed);
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        // RX Visual
        if (ImGui::BeginTabItem("RX Visual"))
        {
            const long long maxBytes = 2LL * 1024 * 1024 * 1024; // 2 GB
            for (auto &iface : ifaces)
            {
                string label = iface.name + "  " + formatBytes(iface.rx.bytes);
                float frac   = (float)min(iface.rx.bytes, maxBytes) / (float)maxBytes;
                ImGui::Text("%-16s", iface.name.c_str()); ImGui::SameLine();
                ImGui::ProgressBar(frac, ImVec2(-1, 0), label.c_str());
            }
            ImGui::EndTabItem();
        }
        // TX Visual
        if (ImGui::BeginTabItem("TX Visual"))
        {
            const long long maxBytes = 2LL * 1024 * 1024 * 1024;
            for (auto &iface : ifaces)
            {
                string label = iface.name + "  " + formatBytes(iface.tx.bytes);
                float frac   = (float)min(iface.tx.bytes, maxBytes) / (float)maxBytes;
                ImGui::Text("%-16s", iface.name.c_str()); ImGui::SameLine();
                ImGui::ProgressBar(frac, ImVec2(-1, 0), label.c_str());
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main(int, char **)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_WindowFlags wf = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window *window = SDL_CreateWindow("System Monitor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, wf);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    bool err = gladLoadGL() == 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD2)
    bool err = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress) == 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
    bool err = false; glbinding::Binding::initialize();
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
    bool err = false;
    glbinding::initialize([](const char *name){ return (glbinding::ProcAddress)SDL_GL_GetProcAddress(name); });
#else
    bool err = false;
#endif
    if (err) { fprintf(stderr, "Failed to initialize OpenGL loader!\n"); return 1; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    // Tweak style for a more polished look
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding   = 6.0f;
    style.FrameRounding    = 4.0f;
    style.ScrollbarRounding= 4.0f;
    style.GrabRounding     = 3.0f;
    style.TabRounding      = 4.0f;
    style.Colors[ImGuiCol_WindowBg]       = ImVec4(0.10f, 0.11f, 0.14f, 1.00f);
    style.Colors[ImGuiCol_Header]         = ImVec4(0.20f, 0.40f, 0.65f, 0.55f);
    style.Colors[ImGuiCol_HeaderHovered]  = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    style.Colors[ImGuiCol_PlotLines]      = ImVec4(0.30f, 0.80f, 0.40f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]  = ImVec4(0.30f, 0.80f, 0.40f, 1.00f);

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImVec4 clear_color = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
    bool done = false;

    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) done = true;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window)) done = true;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

        {
            ImVec2 disp = io.DisplaySize;
            systemWindow("== System ==",
                ImVec2((disp.x / 2) - 10,  (disp.y / 2) + 30),
                ImVec2(10, 10));
            memoryProcessesWindow("== Memory and Processes ==",
                ImVec2((disp.x / 2) - 20, (disp.y / 2) + 30),
                ImVec2((disp.x / 2) + 10, 10));
            networkWindow("== Network ==",
                ImVec2(disp.x - 20, (disp.y / 2) - 60),
                ImVec2(10, (disp.y / 2) + 50));
        }

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
        
        // Low-Frequency Mode: 20 FPS is plenty for a monitor.
        // This drastically reduces CPU and Power consumption.
        SDL_Delay(50); 
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
