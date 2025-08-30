// ===============================
// main.cpp  (Minimal VPN UI + Logic via vpn_logic.h)
// 覆盖原文件即可编译运行
// 依赖：glad.c 已在项目中，GLFW/ImGui 已配置
// ===============================

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>

// ---- 我们的极简逻辑层（唯一来源）----
#include "vpn_logic.h"            // 你之前已放在 src/vpn_logic.h

// ---- OpenGL / GLFW / ImGui ----
#include <glad/glad.h>            // 确保项目里有 src/glad.c
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>



// 选择一个合适的 GLSL 版本字符串
// 如果你的环境是 OpenGL3.0/3.2，使用 "#version 130" / "#version 150" 都可
static const char* kGlslVersion = "#version 130";

// 处理窗口尺寸变化（可选）
static void glfw_error_callback(int error, const char* description) {
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main() {
    // ========== 初始化 GLFW ==========
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to init GLFW\n");
        return EXIT_FAILURE;
    }

    // OpenGL 上下文提示（你也可以改为 3.2 Core）
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 如果用 3.2+
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // macOS 需要

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Minimal VPN UI", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // 开启垂直同步

    // ========== 初始化 GLAD ==========
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::fprintf(stderr, "Failed to init GLAD\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // ========== 初始化 ImGui ==========
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // 外观主题（可选）
    ImGui::StyleColorsDark();


    // 保留默认英文字体（可要可不要）
    io.Fonts->AddFontDefault();

    // 加一个中文字体（任选其一，路径存在即可）
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 18.0f, nullptr,
        io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    // 或者：
    // io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\simhei.ttf", 18.0f, nullptr,
    //                              io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    // io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\simsun.ttc", 18.0f, nullptr,
    //                              io.Fonts->GetGlyphRangesChineseSimplifiedCommon());

    // 加载中文字体，并把它设为默认字体
    ImFont* zh = io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\msyh.ttc",   // 或 simhei.ttf / simsun.ttc
        18.0f,
        nullptr,
        io.Fonts->GetGlyphRangesChineseSimplifiedCommon()
    );
    io.FontDefault = zh;  // 关键：让中文字体成为默认字体


    // 后端绑定
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(kGlslVersion);

    // ========== 我们的 VPN 逻辑模型 ==========
    static VpnModel vpn; // 界面与逻辑通过这个模型交互

    // ========== 主循环 ==========
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // 每帧推进一次“假后端”状态机（方案B来自 vpn_logic.h）
        Tick(vpn);

        // ---- 开始新帧（ImGui）----
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 你可以自定义布局，这里只放一个简单窗口演示
        {
            ImGui::Begin("Minimal VPN UI");

            // 标题分隔（若你的 ImGui 版本不支持 SeparatorText，可换成 Text+Separator）
#if IMGUI_VERSION_NUM >= 18900
            ImGui::SeparatorText("VPN Control (MVP-0)");
#else
            ImGui::TextUnformatted("VPN Control (MVP-0)");
            ImGui::Separator();
#endif

            // 状态文本
            ImGui::Text("State: %s", ToCString(vpn.state));

            // 连接/断开 按钮
            if (vpn.state == VpnState::Disconnected || vpn.state == VpnState::Failed) {
                if (ImGui::Button("Connect")) {
                    StartConnect(vpn);
                }
            }
            else {
                if (ImGui::Button("Disconnect")) {
                    StartDisconnect(vpn);
                }
            }

            ImGui::Dummy(ImVec2(0, 8));
            ImGui::TextUnformatted("Logs");
            ImGui::BeginChild("##vpn_logs", ImVec2(0, 200), true, ImGuiWindowFlags_NoMove);
            for (const auto& line : vpn.logs) {
                ImGui::TextUnformatted(line.c_str());
            }
            ImGui::EndChild();

            ImGui::End();
        }

        // ---- 渲染提交 ----
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.08f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // ========== 清理 ==========
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
