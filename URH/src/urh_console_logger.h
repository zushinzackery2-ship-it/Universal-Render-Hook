#pragma once

#include <Windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

namespace ConsoleLogger
{

inline std::mutex& GetLogMutex()
{
    static std::mutex mutex;
    return mutex;
}

inline std::wstring GetLogFilePath()
{
    static std::wstring cachedPath;
    if (!cachedPath.empty())
    {
        return cachedPath;
    }

    wchar_t modulePath[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    std::wstring path(modulePath, length);
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
    {
        path.resize(slash + 1);
    }
    else
    {
        path.clear();
    }

    path += L"rei_internal.log";
    cachedPath = path;
    return cachedPath;
}

inline void AppendUtf8Line(const char* text)
{
    const std::wstring path = GetLogFilePath();
    HANDLE fileHandle = CreateFileW(
        path.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        return;
    }

    DWORD written = 0;
    const DWORD length = static_cast<DWORD>(std::strlen(text));
    WriteFile(fileHandle, text, length, &written, nullptr);
    CloseHandle(fileHandle);
}

inline void Log(const char* format, ...)
{
    char buffer[1024] = {};

    va_list args;
    va_start(args, format);
    _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
    va_end(args);

    char finalBuffer[1152] = {};
    _snprintf_s(finalBuffer, sizeof(finalBuffer), _TRUNCATE, "[URH] %s\n", buffer);
    std::lock_guard<std::mutex> lock(GetLogMutex());
    std::printf("%s", finalBuffer);
    OutputDebugStringA(finalBuffer);
    AppendUtf8Line(finalBuffer);
}

} // namespace ConsoleLogger
