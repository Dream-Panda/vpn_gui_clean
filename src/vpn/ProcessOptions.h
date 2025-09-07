#pragma once
#include <string>
#include <vector>

struct ProcessOptions {
    std::wstring exe;                    // 可执行文件全路径
    std::vector<std::wstring> args;      // 参数
    std::wstring workingDir;             // 可空
    bool inheritHandles{ false };
    bool hidden{ true };
};
