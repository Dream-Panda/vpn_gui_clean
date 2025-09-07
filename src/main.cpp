// main.cpp
#include <cstdio>
#include <string>
#include <stdexcept>

#ifndef APIENTRY
#define APIENTRY
#endif

// --- OpenGL / Window ---
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// --- ImGui ---
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

// --- Your modules ---
#include "vpn/OpenVpnRunner.h"  // 如果你放在 src/core/，改成 "core/OpenVpnRunner.h"
#include "ui/Panels.h"          // 同理，按你的实际路径调整

// --------------- Globals ---------------
static GLFWwindow* g_Window = nullptr;
static int g_Width = 1280, g_Height = 720;

// VPN globals
static OpenVpnRunner g_vpn;
static LogBuffer     g_log;
static OpenVpnConfig g_cfg{
    /* openvpnExe */ "C:/Program Files/OpenVPN/bin/openvpn.exe",
    /* ovpnFile   */ "C:/Users/Panda Dream 2024/Downloads/jp-tok.prod.surfshark.comsurfshark_openvpn_tcp.ovpn",  // ← 改成你的 .ovpn 绝对路径
    /* workDir    */ "C:/vpn/temp",                // ← 需要可写
    /* authUser   */ "",                           // ← 若需要用户名密码，填这里
    /* authPass   */ "",
    /* verb       */ 3
};

// --------------- Helpers ----------------
static void GlfwErrorCallback(int error, const char* desc) {
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, desc);
}

static void InitGlfwAndWindow() {
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit()) throw std::runtime_error("Failed to init GLFW");

    // OpenGL 3.3 Core (和 ImGui OpenGL3 后端匹配)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef _WIN32
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#endif

    g_Window = glfwCreateWindow(g_Width, g_Height, "VPN GUI (MVP)", nullptr, nullptr);
    if (!g_Window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create window");
    }
    glfwMakeContextCurrent(g_Window);
    glfwSwapInterval(1); // VSYNC
}

static void InitGlad() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        throw std::runtime_error("Failed to init GLAD");
}

static void InitImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // 风格你也可以改成 Dark/Light
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(g_Window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

static void Cleanup() {
    // 停 VPN 进程（若在跑）
    if (g_vpn.running()) g_vpn.stop();

    // ImGui 清理
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // 窗口/GLFW
    if (g_Window) {
        glfwDestroyWindow(g_Window);
        g_Window = nullptr;
    }
    glfwTerminate();
}

// --------------- UI Drawing --------------
static void DrawUI() {
    // 顶栏
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Quit", "Esc")) {
                glfwSetWindowShouldClose(g_Window, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // VPN 控制 + 日志
    UiPanels::DrawVpnControls(
        g_vpn.running(),
        []() { // onStart
            g_log.clear();
            g_vpn.start(g_cfg, [](const std::string& line) { g_log.push(line); });
        },
        []() { // onStop
            g_vpn.stop();
            g_log.push("--- stopped ---");
        }
    );
    UiPanels::DrawLogs(g_log);

    // 可选：一个占位卡片（以后放 Profiles/Settings 等）
    ImGui::Begin("Tips");
    ImGui::TextUnformatted("Fill your OpenVPN exe & .ovpn path in g_cfg above.\n"
        "Then click 'Start OpenVPN' to stream logs here.");
    ImGui::End();
}

// --------------- Main --------------------
int main() {
    try {
        InitGlfwAndWindow();
        InitGlad();
        InitImGui();

        // 主循环
        while (!glfwWindowShouldClose(g_Window)) {
            glfwPollEvents();
            // Esc 退出
            if (glfwGetKey(g_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(g_Window, GLFW_TRUE);

            // 开始新帧
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // --- UI ---
            DrawUI();

            // 渲染
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(g_Window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.08f, 0.10f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(g_Window);
        }
    }
    catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal: %s\n", e.what());
    }

    Cleanup();
    return 0;
}
