// sender.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "sender.h"
#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
#include <unordered_set>
#include <codecvt>
#include <thread>

#define MAX_LOADSTRING 100
#define SERVER_IP "192.168.121.6"
#define SERVER_PORT 9529

// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名

std::unordered_set<tstring> ipSet;
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
   PrintDebugString(_T("Success: 检查自身窗口句柄：%x"), hWnd);
#define KEY_HWND _T("hwnd@sender")
   TCHAR szHwnd[16] = { 0 };
   _stprintf_s(szHwnd, _T("%x"), hWnd);
   if (!_AddEnv(KEY_HWND, szHwnd)) {
       PrintDebugString(_T("Success: 窗口句柄写入环境变量失败：%d"), GetLastError());
   }
   
   std::thread([]() {
       //创建与服务器通信的socket
       WSADATA wsaData;
       WSAStartup(MAKEWORD(2, 2), &wsaData);
       sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
       struct sockaddr_in sockAddr = { 0 };
       sockAddr.sin_family = AF_INET;
       inet_pton(AF_INET, SERVER_IP, &sockAddr.sin_addr);
       sockAddr.sin_port = htons(SERVER_PORT);
       connect(sock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR));
       PrintDebugString(_T("Success: 连接服务器成功[%s:%d]"), SERVER_IP, SERVER_PORT);
   }).detach();
   
   
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
        //std::cerr << e.what() << std::endl;
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
        //std::cout << e.what() << std::endl;
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
            PrintDebugString(_T("Success: 获得ipv4地址：%s"), cAddr);

            if (ipSet.insert(cAddr).second) {
                PrintDebugString(_T("Success: IPSetSize: %d"), ipSet.size());
                //std::string data = UnicodeToUTF8(cAddr);
                send(sock, UnicodeToUTF8(cAddr).c_str(), sizeof(cAddr), 0);
            }
        }
        else if (wParam == 6) { //ipv6
            IN6_ADDR* pAddr = (IN6_ADDR*)pCopyData->lpData;
            TCHAR cAddr[46];
            InetNtop(AF_INET6, pAddr, cAddr, 46);
            PrintDebugString(_T("Success: 获得ipv6地址：%s"), cAddr);
            
            if (ipSet.insert(cAddr).second) {
                PrintDebugString(_T("Success: IPSetSize: %d"), ipSet.size());
                //std::string data = UnicodeToUTF8(cAddr);
                send(sock, UnicodeToUTF8(cAddr).c_str(), sizeof(cAddr), 0);
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
