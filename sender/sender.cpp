// sender.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "sender.h"
#include "utils.h"
#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
#include <unordered_set>
#include <codecvt>
#include <fstream>


#define MAX_LOADSTRING 100
#define FILE_CONFIG _T("sender.config")
#define HWND_ENVKEY _T("hwnd")


// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名

std::unordered_set<tstring> ipSet;
tstring g_strIp;
int g_iPort;
SOCKET sock;


// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 在此处放置代码。
    // 防止多开，节省socket资源，创建全局互斥对象，因为目前需求不需要拦截SYSTEM权限进程
    HANDLE hMutex = CreateMutex(NULL, FALSE, _T("Global\\防止多开，节省socket资源"));
    if (!hMutex) {
        PrintDebugString(false, _T("CreateMutex:%s"), ErrWrap{}().c_str());
        return 0;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        PrintDebugString(false, _T("不能打开多个sender"));
        CloseHandle(hMutex);
        return 0;
    }
    
    // 从配置文件获取服务器地址
    tifstream ifs(FILE_CONFIG);
    if (ifs.fail()) {
        PrintDebugString(false, _T("文件打开失败[file:%s, error:%s]"), FILE_CONFIG, ErrWrap{}().c_str());
        return 0;
    }
    tstring strIPort;
    std::getline(ifs, strIPort);
    ifs.close();
    PrintDebugString(true, _T("从文件[%s]读取的服务器地址：%s"), FILE_CONFIG, strIPort.c_str());
    size_t segIndex = strIPort.find_first_of(_T(':'));
    if (segIndex == tstring::npos) {
        PrintDebugString(false, _T("服务器地址[%s]没有冒号分隔符"), strIPort.c_str());
        return 0;
    }
    g_strIp = strIPort.substr(0, segIndex);
    _stscanf_s(strIPort.substr(segIndex + 1).c_str(), _T("%d"), &g_iPort);
    PrintDebugString(true, _T("分割后的服务器地址[ip:%s, port:%d]"), g_strIp.c_str(), g_iPort);

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_SENDER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SENDER));

    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SENDER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_SENDER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 将实例句柄存储在全局变量中

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   //先把窗口句柄存环境变量
   PrintDebugString(true, _T("检查自身窗口句柄：%x"), hWnd);
   TCHAR szHwnd[16];
   _stprintf_s(szHwnd, _T("%x"), hWnd);
   if (SetEnvironmentVariable(HWND_ENVKEY, szHwnd) == 0) {
       PrintDebugString(false, _T("sender窗口句柄写入环境变量失败：%s"), ErrWrap{}().c_str());
   }

   //连接服务器
   WSADATA wsaData;
   PrintDebugString(true, _T("WSAStartup[ret:%d]"), WSAStartup(MAKEWORD(2, 2), &wsaData));
   /*WSASYSNOTREADY: The underlying network subsystem is not ready for network communication.
   WSAVERNOTSUPPORTED: The version of Windows Sockets support requested is not provided by this particular Windows Sockets implementation.
   WSAEINPROGRESS: A blocking Windows Sockets 1.1 operation is in progress.
   WSAEPROCLIM: A limit on the number of tasks supported by the Windows Sockets implementation has been reached.
   WSAEFAULT: The lpWSAData parameter is not a valid pointer.*/
   sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   struct sockaddr_in sockAddr = { 0 };
   sockAddr.sin_family = AF_INET;
   InetPton(AF_INET, g_strIp.c_str(), &sockAddr.sin_addr);
   sockAddr.sin_port = htons(g_iPort);
   connect(sock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR));
   PrintDebugString(true, _T("连接服务器成功[%s:%d]"), g_strIp.c_str(), g_iPort);

   
   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
std::string UnicodeToUTF8(const std::wstring& wstr)
{
    std::string ret;
    try {
        std::wstring_convert< std::codecvt_utf8<wchar_t> > wcv;
        ret = wcv.to_bytes(wstr);
    }
    catch (const std::exception& e) {
        PrintDebugStringA(false, "UnicodeToUTF8: %s", e.what());
    }
    return ret;
}

std::wstring UTF8ToUnicode(const std::string& str)
{
    std::wstring ret;
    try {
        std::wstring_convert< std::codecvt_utf8<wchar_t> > wcv;
        ret = wcv.from_bytes(str);
    }
    catch (const std::exception& e) {
        PrintDebugStringA(false, "UTF8ToUnicode: %s", e.what());
    }
    return ret;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COPYDATA: {
        COPYDATASTRUCT* pCopyData = reinterpret_cast<COPYDATASTRUCT*>(lParam);
        if (wParam == 4) {  //ipv4
            IN_ADDR* pAddr = (IN_ADDR*)pCopyData->lpData;
            TCHAR cAddr[16];
            InetNtop(AF_INET, pAddr, cAddr, 16);
            PrintDebugString(true, _T("获得ipv4地址：%s"), cAddr);

            if (ipSet.insert(cAddr).second) {
                PrintDebugString(true, _T("IPSetSize: %d"), ipSet.size());
                std::string data = UnicodeToUTF8(tstring(L"<") + cAddr + L'>');
                if (SOCKET_ERROR == send(sock, data.c_str(), sizeof(char) * data.length(), 0)) {
                    PrintDebugString(false, _T("send函数错误[%s]"), ErrWrap{}(WSAGetLastError()).c_str());
                }
                /*char recvBuf[32];
                recv(sock, recvBuf, sizeof(recvBuf), 0);
                PrintDebugString(true, _T("recv: %s"), UTF8ToUnicode(recvBuf).c_str());*/
            }
        }
        else if (wParam == 6) { //ipv6
            IN6_ADDR* pAddr = (IN6_ADDR*)pCopyData->lpData;
            TCHAR cAddr[46];
            InetNtop(AF_INET6, pAddr, cAddr, 46);
            PrintDebugString(true, _T("获得ipv6地址：%s"), cAddr);
            
            if (ipSet.insert(cAddr).second) {
                PrintDebugString(true, _T("IPSetSize: %d"), ipSet.size());
                std::string data = UnicodeToUTF8(tstring(L"<") + cAddr + L'>');
                if (SOCKET_ERROR == send(sock, data.c_str(), sizeof(char) * data.length(), 0)) {
                    PrintDebugString(false, _T("send函数错误[%s]"), ErrWrap{}(WSAGetLastError()).c_str());
                }
                /*char recvBuf[32];
                recv(sock, recvBuf, sizeof(recvBuf), 0);
                PrintDebugString(true, _T("recv: %s"), UTF8ToUnicode(recvBuf).c_str());*/
            }
        }

        break;
    }

    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 分析菜单选择:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: 在此处添加使用 hdc 的任何绘图代码...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
