#pragma once
#include <string>
#include <vector>

struct ProcessOptions {
    std::wstring exe;                    // ��ִ���ļ�ȫ·��
    std::vector<std::wstring> args;      // ����
    std::wstring workingDir;             // �ɿ�
    bool inheritHandles{ false };
    bool hidden{ true };
};
