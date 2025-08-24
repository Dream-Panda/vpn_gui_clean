// Enhanced Minimal VPN UI (ImGui + GLFW + GLAD)
// Now with initial real-engine hookup via sing-box for VMess & Hysteria2 on Windows.
//
// What you get:
// - Protocols: VMess, Hysteria2 (plus placeholders for WireGuard/OpenVPN/SS)
// - Per-protocol fields in UI (UUID/WS-Path/SNI for VMess; password/SNI for Hysteria2)
// - One-click: Generate sing-box config (sb.json) and spawn sing-box as a child process
// - Live log tailing from child process stdout/stderr into the ImGui log pane
// - Stop button terminates the child process
//
// Requirements (Windows):
// - Put sing-box.exe next to your .exe (same working directory)
// - Project still links: glfw3, opengl32, glad, imgui backends
// - In Properties -> C/C++ -> Command Line: add /utf-8
// - If MSVC warns about fopen: define _CRT_SECURE_NO_WARNINGS in project
//
// NOTE: This sample focuses on Windows for process management. On Linux/macOS you can adapt by using popen/posix_spawn and pipes.

// 解决 Windows API 宏冲突和警告
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <deque>
#include <vector>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <thread>
#include <cmath>

#ifdef _WIN32
#  include <windows.h>
#endif

#ifdef _WIN32
#  define PATH_SEP "\\\\"
#else
#  define PATH_SEP "/"
#endif

enum class ConnState { Disconnected, Connecting, Connected };

// Small helper: format current local time like "12:34:56"
static std::string NowHHMMSS() {
    using clock = std::chrono::system_clock;
    auto t = clock::to_time_t(clock::now());
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

struct RingLog {
    std::deque<std::string> lines;
    size_t max_lines = 1000; // keep last N lines
    void clear() { lines.clear(); }
    void push(const std::string& s) {
        if (lines.size() >= max_lines) lines.pop_front();
        lines.push_back(s);
    }
};

#ifdef _WIN32
struct ChildProc {
    PROCESS_INFORMATION pi{};
    HANDLE hRead = nullptr;   // stdout/stderr read handle
    HANDLE hWrite = nullptr;  // stdout/stderr write handle (closed in parent after spawn)
    std::thread reader;
    std::atomic<bool> running{false};
};
#endif

struct App {
    // Common UI
    char server[128] = "";
    int port = 0;
    int proto_idx = 0; // 0:WireGuard 1:OpenVPN 2:Shadowsocks 3:VMess 4:Hysteria2 5:Custom
    ConnState state = ConnState::Disconnected;
    std::string status = "Disconnected";
    RingLog logbuf;
    std::chrono::steady_clock::time_point t_connect_start{};
    float ui_scale = 1.0f; // DPI scale multiplier

    // VMess (WS+TLS) fields
    char vmess_uuid[64] = "";
    bool vmess_tls = true;
    char vmess_sni[128] = "";      // TLS ServerName / Host header
    char vmess_ws_path[128] = "/"; // WebSocket path

    // Hysteria2 fields
    char hy2_password[128] = "";
    char hy2_sni[128] = "";        // TLS SNI
    int  hy2_up_mbps = 10;
    int  hy2_down_mbps = 50;

#ifdef _WIN32
    ChildProc child; // sing-box child process
#endif

    void log(const std::string& msg) {
        logbuf.push(NowHHMMSS() + "  " + msg);
        status = msg;
    }

    void set_state(ConnState s) {
        state = s;
    }
};

static void ApplyMinimalTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 10.0f;
    s.FrameRounding = 8.0f;
    s.GrabRounding  = 8.0f;
    s.ScrollbarRounding = 8.0f;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    c[ImGuiCol_Text]     = ImVec4(0.92f, 0.92f, 0.92f, 1.0f);
    c[ImGuiCol_FrameBg]  = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
    c[ImGuiCol_Button]   = ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.32f, 0.32f, 0.32f, 1.0f);
    c[ImGuiCol_ButtonActive]  = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
}

static void DrawStatusBadge(ConnState st) {
    ImU32 col = IM_COL32(180, 180, 180, 255);
    const char* label = "[ ] Disconnected";
    if (st == ConnState::Connecting) { col = IM_COL32(255, 200, 0, 255); label = "[~] Connecting"; }
    if (st == ConnState::Connected)  { col = IM_COL32(0, 200, 120, 255);  label = "[*] Connected";  }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4((col>>0 & 0xFF)/255.0f, (col>>8 & 0xFF)/255.0f, (col>>16 & 0xFF)/255.0f, 1.0f),
                       "%s", label);
}

static void glfw_error_callback(int error, const char* desc) {
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, desc);
}

// --- Simple config save/load (binary-plain) for a few fields
static bool SaveConfig(const App& app, const char* path) {
    FILE* f = nullptr;
#ifdef _WIN32
    if (fopen_s(&f, path, "wb") != 0 || !f) return false;
#else
    f = std::fopen(path, "wb");
    if (!f) return false;
#endif
    std::fwrite(app.server, 1, sizeof(app.server), f);
    std::fwrite(&app.port, 1, sizeof(app.port), f);
    std::fwrite(&app.proto_idx, 1, sizeof(app.proto_idx), f);
    std::fwrite(&app.ui_scale, 1, sizeof(app.ui_scale), f);
    std::fwrite(app.vmess_uuid, 1, sizeof(app.vmess_uuid), f);
    std::fwrite(&app.vmess_tls, 1, sizeof(app.vmess_tls), f);
    std::fwrite(app.vmess_sni, 1, sizeof(app.vmess_sni), f);
    std::fwrite(app.vmess_ws_path, 1, sizeof(app.vmess_ws_path), f);
    std::fwrite(app.hy2_password, 1, sizeof(app.hy2_password), f);
    std::fwrite(app.hy2_sni, 1, sizeof(app.hy2_sni), f);
    std::fwrite(&app.hy2_up_mbps, 1, sizeof(app.hy2_up_mbps), f);
    std::fwrite(&app.hy2_down_mbps, 1, sizeof(app.hy2_down_mbps), f);
    std::fclose(f);
    return true;
}

static bool LoadConfig(App& app, const char* path) {
    FILE* f = nullptr;
#ifdef _WIN32
    if (fopen_s(&f, path, "rb") != 0 || !f) return false;
#else
    f = std::fopen(path, "rb");
    if (!f) return false;
#endif
    size_t n = 0;
    n += std::fread(app.server, 1, sizeof(app.server), f);
    n += std::fread(&app.port, 1, sizeof(app.port), f);
    n += std::fread(&app.proto_idx, 1, sizeof(app.proto_idx), f);
    n += std::fread(&app.ui_scale, 1, sizeof(app.ui_scale), f);
    n += std::fread(app.vmess_uuid, 1, sizeof(app.vmess_uuid), f);
    n += std::fread(&app.vmess_tls, 1, sizeof(app.vmess_tls), f);
    n += std::fread(app.vmess_sni, 1, sizeof(app.vmess_sni), f);
    n += std::fread(app.vmess_ws_path, 1, sizeof(app.vmess_ws_path), f);
    n += std::fread(app.hy2_password, 1, sizeof(app.hy2_password), f);
    n += std::fread(app.hy2_sni, 1, sizeof(app.hy2_sni), f);
    n += std::fread(&app.hy2_up_mbps, 1, sizeof(app.hy2_up_mbps), f);
    n += std::fread(&app.hy2_down_mbps, 1, sizeof(app.hy2_down_mbps), f);
    std::fclose(f);
    return n > 0;
}

static void WriteTextFile(const std::string& path, const std::string& text) {
    FILE* f = nullptr;
#ifdef _WIN32
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return;
#else
    f = std::fopen(path.c_str(), "wb");
    if (!f) return;
#endif
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
}

// --- sing-box config emitters ---
static std::string EmitSingBoxVMessWS(const App& a) {
    // Minimal local socks(10808)+http(10809) inbound, one vmess outbound (ws+tls optional)
    std::ostringstream o;
    o << "{\n"
      << "  \"log\": { \"disabled\": false, \"level\": \"info\" },\n"
      << "  \"inbounds\": [\n"
      << "    { \"type\": \"socks\", \"listen\": \"127.0.0.1\", \"listen_port\": 10808 },\n"
      << "    { \"type\": \"http\",  \"listen\": \"127.0.0.1\", \"listen_port\": 10809 }\n"
      << "  ],\n"
      << "  \"outbounds\": [\n"
      << "    {\n"
      << "      \"type\": \"vmess\",\n"
      << "      \"server\": \"" << a.server << "\",\n"
      << "      \"server_port\": " << a.port << ",\n"
      << "      \"uuid\": \"" << a.vmess_uuid << "\",\n"
      << "      \"security\": \"auto\",\n"
      << "      \"transport\": {\n"
      << "        \"type\": \"ws\",\n"
      << "        \"path\": \"" << a.vmess_ws_path << "\"\n"
      << "      },\n"
      << "      \"tls\": { \"enabled\": " << (a.vmess_tls? "true":"false")
      << ", \"server_name\": \"" << a.vmess_sni << "\" }\n"
      << "    }\n"
      << "  ]\n"
      << "}\n";
    return o.str();
}

static std::string EmitSingBoxHysteria2(const App& a) {
    std::ostringstream o;
    o << "{\n"
      << "  \"log\": { \"disabled\": false, \"level\": \"info\" },\n"
      << "  \"inbounds\": [\n"
      << "    { \"type\": \"socks\", \"listen\": \"127.0.0.1\", \"listen_port\": 10808 },\n"
      << "    { \"type\": \"http\",  \"listen\": \"127.0.0.1\", \"listen_port\": 10809 }\n"
      << "  ],\n"
      << "  \"outbounds\": [\n"
      << "    {\n"
      << "      \"type\": \"hysteria2\",\n"
      << "      \"server\": \"" << a.server << "\",\n"
      << "      \"server_port\": " << a.port << ",\n"
      << "      \"password\": \"" << a.hy2_password << "\",\n"
      << "      \"tls\": { \"enabled\": true, \"server_name\": \"" << a.hy2_sni << "\" },\n"
      << "      \"up_mbps\": " << a.hy2_up_mbps << ",\n"
      << "      \"down_mbps\": " << a.hy2_down_mbps << "\n"
      << "    }\n"
      << "  ]\n"
      << "}\n";
    return o.str();
}

#ifdef _WIN32
// Spawn sing-box and capture stdout/stderr. Return true if started.
static bool SpawnSingBox(App& app, const std::string& cmdline, const char* workdir = nullptr) {
    if (app.child.running.load()) return false;

    SECURITY_ATTRIBUTES sa{}; sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE; sa.lpSecurityDescriptor = nullptr;
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) { app.log("CreatePipe failed"); return false; }
    // Ensure our read handle is not inherited by the child
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{}; si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError  = hWrite;

    PROCESS_INFORMATION pi{};

    // Create mutable command line
    std::string cmd = cmdline; // e.g. "sing-box.exe run -c sb.json"
    std::vector<char> cl(cmd.begin(), cmd.end());
    cl.push_back('\0');

    BOOL ok = CreateProcessA(
        nullptr,            // application name
        cl.data(),          // command line
        nullptr, nullptr,   // process/thread security
        TRUE,               // inherit handles (so child sees stdout/stderr pipe)
        CREATE_NO_WINDOW,   // no console window
        nullptr,            // env
        workdir,            // cwd
        &si, &pi);

    // Parent no longer needs the write end
    CloseHandle(hWrite);

    if (!ok) {
        CloseHandle(hRead);
        app.log("CreateProcess failed");
        return false;
    }

    app.child.hRead = hRead;
    app.child.pi = pi;
    app.child.running = true;

    // Reader thread
    app.child.reader = std::thread([&app]() {
        char buf[4096]; DWORD n = 0; std::string acc;
        while (app.child.running.load()) {
            BOOL r = ReadFile(app.child.hRead, buf, sizeof(buf)-1, &n, nullptr);
            if (!r || n == 0) { Sleep(50); continue; }
            buf[n] = 0;
            acc += buf;
            // split lines
            size_t pos = 0; size_t nl;
            while ((nl = acc.find('\n', pos)) != std::string::npos) {
                std::string line = acc.substr(pos, nl - pos);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                app.log(line);
                pos = nl + 1;
            }
            acc.erase(0, pos);
        }
    });

    app.log("sing-box started");
    return true;
}

static void KillSingBox(App& app) {
    if (!app.child.running.load()) return;
    app.child.running = false;
    if (app.child.hRead) { CloseHandle(app.child.hRead); app.child.hRead = nullptr; }
    if (app.child.pi.hProcess) {
        TerminateProcess(app.child.pi.hProcess, 0);
        WaitForSingleObject(app.child.pi.hProcess, 1500);
        CloseHandle(app.child.pi.hThread);
        CloseHandle(app.child.pi.hProcess);
        app.child.pi = PROCESS_INFORMATION{};
    }
    if (app.child.reader.joinable()) app.child.reader.join();
    app.log("sing-box stopped");
}
#endif

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(840, 640, "Minimal VPN UI", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { glfwDestroyWindow(window); glfwTerminate(); return 1; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ApplyMinimalTheme();

    // Try to scale UI based on monitor content scale
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    float scale = (xscale + yscale) * 0.5f;
    io.FontGlobalScale = scale; // rough heuristic; user-adjustable in UI

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    App app;
    const char* protos[] = { "WireGuard", "OpenVPN", "Shadowsocks", "VMess", "Hysteria2", "Custom" };

    // Config path (cwd)
    const char* cfg_path = "vpn_ui.cfg";
    LoadConfig(app, cfg_path);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save Config", "Ctrl+S")) SaveConfig(app, cfg_path);
                if (ImGui::MenuItem("Load Config")) LoadConfig(app, cfg_path);
                if (ImGui::MenuItem("Exit", "Alt+F4")) glfwSetWindowShouldClose(window, 1);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::SliderFloat("UI Scale", &app.ui_scale, 0.75f, 2.0f, "%.2fx");
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        ImGui::GetIO().FontGlobalScale = scale * app.ui_scale;

        ImGui::SetNextWindowSize(ImVec2(700, 520), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("VPN Panel")) {
            ImGui::Combo("Protocol", &app.proto_idx, protos, IM_ARRAYSIZE(protos));
            ImGui::InputText("Server", app.server, IM_ARRAYSIZE(app.server));
            ImGui::InputInt("Port", &app.port); if (app.port < 1) app.port = 1; if (app.port > 65535) app.port = 65535;

            // Per-protocol controls
            if (app.proto_idx == 3) { // VMess
                ImGui::Separator();
                ImGui::TextDisabled("VMess (WS+TLS) Params");
                ImGui::InputText("UUID", app.vmess_uuid, IM_ARRAYSIZE(app.vmess_uuid));
                ImGui::Checkbox("Enable TLS", &app.vmess_tls);
                ImGui::InputText("SNI/Host", app.vmess_sni, IM_ARRAYSIZE(app.vmess_sni));
                ImGui::InputText("WS Path", app.vmess_ws_path, IM_ARRAYSIZE(app.vmess_ws_path));
            }
            if (app.proto_idx == 4) { // Hysteria2
                ImGui::Separator();
                ImGui::TextDisabled("Hysteria2 Params");
                ImGui::InputText("Password", app.hy2_password, IM_ARRAYSIZE(app.hy2_password));
                ImGui::InputText("SNI", app.hy2_sni, IM_ARRAYSIZE(app.hy2_sni));
                ImGui::InputInt("Up Mbps", &app.hy2_up_mbps);
                ImGui::InputInt("Down Mbps", &app.hy2_down_mbps);
            }

            // Status row
#ifdef _WIN32
            if (app.state == ConnState::Disconnected) {
                if (ImGui::Button("Connect")) {
                    // Emit sb.json
                    std::string json;
                    if (app.proto_idx == 3) json = EmitSingBoxVMessWS(app);
                    else if (app.proto_idx == 4) json = EmitSingBoxHysteria2(app);
                    else json.clear();

                    if (json.empty()) {
                        app.log("Protocol not implemented yet");
                    } else {
                        WriteTextFile("sb.json", json);
                        // Try start sing-box
                        if (SpawnSingBox(app, "sing-box.exe run -c sb.json")) {
                            app.set_state(ConnState::Connecting);
                            app.t_connect_start = std::chrono::steady_clock::now();
                        } else {
                            app.log("Failed to start sing-box");
                        }
                    }
                }
            } else {
                if (ImGui::Button("Disconnect")) {
                    KillSingBox(app);
                    app.set_state(ConnState::Disconnected);
                }
                ImGui::SameLine();
                if (app.state == ConnState::Connecting) {
                    float t = (float)fmod(ImGui::GetTime(), 1.0);
                    ImGui::ProgressBar(0.3f + 0.7f * t, ImVec2(120, 0), "");
                }
            }
#else
            ImGui::TextDisabled("Process control implemented for Windows in this sample.");
#endif
            DrawStatusBadge(app.state);

            // Advance connecting->connected after 3s (simple heuristic)
            if (app.state == ConnState::Connecting) {
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - app.t_connect_start).count();
                if (ms > 3000) app.set_state(ConnState::Connected);
            }

            ImGui::Text("Status: %s", app.status.c_str());

            // Logs
            ImGui::Separator();
            ImGui::TextUnformatted("Log");
            ImGui::SameLine(); if (ImGui::SmallButton("Clear##log")) app.logbuf.clear();
            ImGui::BeginChild("Log", ImVec2(0, 240), true, ImGuiWindowFlags_HorizontalScrollbar);
            for (const auto& line : app.logbuf.lines) ImGui::TextUnformatted(line.c_str());
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            ImGui::Separator();
            ImGui::TextDisabled("Hotkeys:  Ctrl+Enter Connect/Disconnect   Esc Disconnect   Ctrl+L Clear Log   F5 Connect   Ctrl+S Save");
        }
        ImGui::End();

        // Keyboard shortcuts (when window focused)
        ImGuiIO& kio = ImGui::GetIO();
        bool ctrl = kio.KeyCtrl;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
#ifdef _WIN32
            if (app.state == ConnState::Disconnected) {
                // mimic Connect button
                std::string json;
                if (app.proto_idx == 3) json = EmitSingBoxVMessWS(app);
                else if (app.proto_idx == 4) json = EmitSingBoxHysteria2(app);
                if (!json.empty()) {
                    WriteTextFile("sb.json", json);
                    if (SpawnSingBox(app, "sing-box.exe run -c sb.json")) {
                        app.set_state(ConnState::Connecting);
                        app.t_connect_start = std::chrono::steady_clock::now();
                    }
                }
            } else { KillSingBox(app); app.set_state(ConnState::Disconnected); }
#endif
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
#ifdef _WIN32
            KillSingBox(app); app.set_state(ConnState::Disconnected);
#endif
        }
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_L)) app.logbuf.clear();
        if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
#ifdef _WIN32
            if (app.state == ConnState::Disconnected) {
                std::string json;
                if (app.proto_idx == 3) json = EmitSingBoxVMessWS(app);
                else if (app.proto_idx == 4) json = EmitSingBoxHysteria2(app);
                if (!json.empty()) {
                    WriteTextFile("sb.json", json);
                    if (SpawnSingBox(app, "sing-box.exe run -c sb.json")) {
                        app.set_state(ConnState::Connecting);
                        app.t_connect_start = std::chrono::steady_clock::now();
                    }
                }
            }
#endif
        }
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) SaveConfig(app, cfg_path);

        // Render
        ImGui::Render();
        int w, h; glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.10f, 0.10f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

#ifdef _WIN32
    KillSingBox(app);
#endif

    SaveConfig(app, cfg_path);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
