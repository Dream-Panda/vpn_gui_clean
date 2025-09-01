// Minimal GLFW + GLAD + ImGui app with a Debug Process panel.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>
#include <cstdio>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "ProcessRunner.h"

// utf8 -> wide helper (dest buffer must be writable)
static std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) return L"";
    int wlen = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(wlen, 0);
    ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], wlen);
    return w;
}

static void DrawDebugProcessPanel() {
    static ProcessRunner runner;

    ImGui::Begin("Debug / Process");

    static char exeBuf[260] = "C:\\Windows\\System32\\ping.exe";
    static char argsBuf[260] = "1.1.1.1 -n 3";

    ImGui::InputText("exe", exeBuf, IM_ARRAYSIZE(exeBuf));
    ImGui::InputText("args", argsBuf, IM_ARRAYSIZE(argsBuf));

    if (!runner.IsRunning()) {
        if (ImGui::Button("Start")) {
            runner.ClearLog();
            runner.Start(utf8_to_wide(exeBuf), utf8_to_wide(argsBuf));
        }
    }
    else {
        if (ImGui::Button("Stop")) {
            runner.Stop(true);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Running...");
    }

    static std::string log;
    if (auto chunk = runner.ConsumeNewOutput(); !chunk.empty()) {
        log += chunk;
    }

    ImGui::Dummy(ImVec2(0, 8));
    ImGui::TextUnformatted("Logs");
    ImGui::BeginChild("##proc_logs", ImVec2(0, 260), true);
    ImGui::TextUnformatted(log.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    if (ImGui::Button("Clear")) log.clear();

    ImGui::End();
}

static void DrawVpnPanel() {
    ImGui::Begin("VPN");
    ImGui::TextUnformatted("MVP placeholder. Add Profiles/Connect/Logs later.");
    ImGui::End();
}

static void glfw_error_callback(int error, const char* desc) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, desc);
}

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "VPN GUI (MVP)", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::fprintf(stderr, "Failed to init GLAD\n");
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        DrawVpnPanel();
        DrawDebugProcessPanel();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.08f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
