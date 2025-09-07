#pragma once
#include <functional>
#include <string>
#include <vector>
#include "ProcessRunner.h"

struct OpenVpnConfig {
    std::wstring openvpnExe;     // openvpn.exe 路径
    std::wstring ovpnFile;       // 配置文件路径
    std::vector<std::wstring> extraArgs; // 额外参数
};

class OpenVpnRunner {
public:
    bool start(const OpenVpnConfig& cfg,
        std::function<void(const std::string&)> onOutput = {},
        std::function<void(const std::string&)> onError = {});
    void stop();
    bool running() const { return runner_.running(); }

private:
    ProcessRunner runner_;
};
