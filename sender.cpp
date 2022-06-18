// sender.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <Windows.h>
#include <tchar.h>

//自定义宏
#ifdef UNICODE
    #define __FUNC__ __FUNCTIONW__
    #define tstring std::wstring
    #define to_tstring std::to_wstring
    #define ifstream std::wifstream
#else
    #define __FUNC__ __FUNCTION__
    #define tstring std::string
    #define to_tstring std::to_string
    #define ifstream std::ifstream
#endif

bool _AddEnv(const TCHAR* key, const TCHAR* value)
{
    HKEY hKey;
    if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, _T("Environment"), 0, KEY_WRITE, &hKey))
    {
        if (ERROR_SUCCESS == RegSetValueEx(hKey, key, 0, REG_SZ, (PBYTE)value, sizeof(TCHAR) * lstrlen(value)))
        {
            RegCloseKey(hKey);
            return true;
        }
        _tprintf_s(_T("%s.RegSetValueEx(%s:%s): %d\n"), __FUNC__, key, value, GetLastError());
        RegCloseKey(hKey);
    }
    _tprintf_s(_T("%s.RegOpenKeyEx: %d\n"), __FUNC__, GetLastError());
    return false;
}

bool _DelEnv(const TCHAR* key)
{
    HKEY hKey;
    if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, _T("Environment"), 0, KEY_WRITE, &hKey))
    {
        if (ERROR_SUCCESS == RegDeleteValue(hKey, key))
        {
            RegCloseKey(hKey);
            return true;
        }
        _tprintf_s(_T("%s.RegDeleteValue(%s): %d\n"), __FUNC__, key, GetLastError());
        RegCloseKey(hKey);
    }
    _tprintf_s(_T("%s.RegOpenKeyEx: %d\n"), __FUNC__, GetLastError());
    return false;
}

bool _GetEnv(const TCHAR* key, TCHAR* value, DWORD nSize)
{
    HKEY hKey;
    if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, _T("Environment"), 0, KEY_READ, &hKey))
    {
        DWORD size = sizeof(TCHAR) * nSize;
        if (ERROR_SUCCESS == RegQueryValueEx(hKey, key, NULL, NULL, (LPBYTE)value, &size))
        {
            RegCloseKey(hKey);
            return true;
        }
        _tprintf_s(_T("%s.RegQueryValueEx(%s): %d\n"), __FUNC__, key, GetLastError());
        RegCloseKey(hKey);
    }
    _tprintf_s(_T("%s.RegOpenKeyEx: %d\n"), __FUNC__, GetLastError());//如何将每个api与它的描述字符对应起来
    return false;
}

int main()
{
    //先把线程id存环境变量
#define TID_KEY _T("tid@sender")
    TCHAR szTid[8];
    _stprintf_s(szTid, _T("%d"), GetCurrentThreadId());
    _AddEnv(TID_KEY, szTid);


    MSG msg;
    //PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (msg.message == WM_APP) {
            std::cout << "Hello World!\n";
        }
        /*if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }*/
    }

    return 0;
}