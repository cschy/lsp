// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "utils.h"
#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")
#include <WS2spi.h>
#include <tchar.h>
#include <WS2tcpip.h>
#include <fstream>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")


#define MAX_PROC_COUNT 16
#define MAX_PROC_NAME 16
#define CONFIG_FILE _T("lsp.config")
#define SENDER_FILE _T("sender.exe")
//#define REGDIR_ENV _T("Environment")
//#define KEY_SENDER_HWND _T("hwnd@sender")


TCHAR	g_szCurrentApp[MAX_PATH] = { 0 };	//当前调用本DLL的程序名称
WSPPROC_TABLE g_NextProcTable;				//下层函数列表


#pragma data_seg (".shared")
TCHAR g_szHookProc[MAX_PROC_COUNT][MAX_PROC_NAME] = { 0 };
TCHAR g_szDllDir[MAX_PATH] = { 0 };
unsigned int iCurrentProcNum = 0;
HWND hSenderWnd = NULL;
#pragma data_seg ()
#pragma comment (linker, "/section:.shared,rws")

BOOL CALLBACK EnumThreadWndProc(HWND hwnd, LPARAM lParam)
{
	//确保sender在载入当前dll之前已创建窗口
	hSenderWnd = hwnd;
	PrintDebugString(true, _T("在dll中枚举到的窗口句柄：%x"), hSenderWnd);
	return FALSE;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
	case DLL_PROCESS_ATTACH: {
		//资源一次初始化
		if (g_szDllDir[0] == _T('\0')) {
			GetModuleFileName(hModule, g_szDllDir, MAX_PATH);
			PathRemoveFileSpec(g_szDllDir);

			//开启sender进程，尽量让此操作靠前，以确保在拦截ip时已获取sender的窗口句柄
			TCHAR szSenderPath[MAX_PATH];
			lstrcpy(szSenderPath, g_szDllDir);
			PathAppend(szSenderPath, SENDER_FILE);
			STARTUPINFO si = { 0 };
			PROCESS_INFORMATION pi = { 0 };
			si.cb = sizeof(STARTUPINFO);
			si.wShowWindow = SW_HIDE;
			si.dwFlags = STARTF_USESHOWWINDOW;
			if (CreateProcess(NULL, szSenderPath, NULL, NULL, FALSE, 0, NULL, g_szDllDir, &si, &pi))
			{
				PrintDebugString(true, _T("创建进程[%s]"), szSenderPath);
			}
			else {
				PrintDebugString(false, _T("创建进程[%s]：%s"), szSenderPath, ErrWrap{}().c_str());
			}
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			//Sleep(500);//等注册表写好，也可以等待sender里SetEvent
			//从注册表获取sender窗口句柄
			/*TCHAR szSenderHwnd[16] = { 0 };
			bool ret = RegWrap{ HKEY_CURRENT_USER, REGDIR_ENV }.get(KEY_SENDER_HWND, szSenderHwnd, sizeof(szSenderHwnd));
			_stscanf_s(szSenderHwnd, _T("%x"), &hSenderWnd);
			PrintDebugString(ret, _T("得到sender的窗口句柄：%x"), hSenderWnd);*/

			//从文件加载需要拦截的应用程序
			TCHAR szCfgFilePath[MAX_PATH];
			lstrcpy(szCfgFilePath, g_szDllDir);
			PathAppend(szCfgFilePath, CONFIG_FILE);
			PrintDebugString(true, _T("配置文件路径: %s"), szCfgFilePath);
			tifstream ifs(szCfgFilePath);
			int i = 0;
			while (ifs.getline(g_szHookProc[i], MAX_PROC_NAME)) {
				PrintDebugString(true, _T("拦截进程：%s"), g_szHookProc[i]);
				i++;
				if (i >= MAX_PROC_COUNT) {
					break;
				}
			}
			iCurrentProcNum = i;
			ifs.close();
		}
	}
		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

LPWSAPROTOCOL_INFOW GetProvider(LPINT lpTotalProtocols)
{
	DWORD dwSize = 0;
	int nError;
	LPWSAPROTOCOL_INFOW pProtoInfo = NULL;

	// 取得需要的长度
	if (WSCEnumProtocols(NULL, pProtoInfo, &dwSize, &nError) == SOCKET_ERROR)
	{
		if (nError != WSAENOBUFS) {
			PrintDebugString(false, _T("WSCEnumProtocols:%s"), ErrWrap{}(nError).c_str());
			return NULL;
		}
	}

	pProtoInfo = (LPWSAPROTOCOL_INFOW)GlobalAlloc(GPTR, dwSize);
	*lpTotalProtocols = WSCEnumProtocols(NULL, pProtoInfo, &dwSize, &nError);
	return pProtoInfo;
}

void FreeProvider(LPWSAPROTOCOL_INFOW pProtoInfo)
{
	GlobalFree(pProtoInfo);
}

int WSPAPI WSPConnect(SOCKET s, const struct sockaddr FAR* name, int namelen, LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS, LPQOS lpGQOS, LPINT lpErrno)
{
	for (int i = 0; i < iCurrentProcNum; i++)
	{
		if (lstrcmp(g_szCurrentApp, g_szHookProc[i]) == 0)
		{
			if (namelen == sizeof(sockaddr_in)) {
				sockaddr_in* addr = (sockaddr_in*)name;
				TCHAR cAddr[16] = { 0 };
				InetNtop(AF_INET, &addr->sin_addr, cAddr, 16);
				PrintDebugString(true, _T("%s.connect[%s:%d]"), g_szCurrentApp, cAddr, ntohs(addr->sin_port));
				
				COPYDATASTRUCT copyData;
				copyData.lpData = &addr->sin_addr;
				copyData.cbData = sizeof(addr->sin_addr);
				SendMessage(hSenderWnd, WM_COPYDATA, 4, (LPARAM)&copyData);
			}
			else if (namelen == sizeof(sockaddr_in6)) {
				sockaddr_in6* addr = (sockaddr_in6*)name;
				TCHAR cAddr[46] = { 0 };
				InetNtop(AF_INET6, &addr->sin6_addr, cAddr, 46);
				PrintDebugString(true, _T("%s.connect[%s:%d]"), g_szCurrentApp, cAddr, ntohs(addr->sin6_port));

				COPYDATASTRUCT copyData;
				copyData.lpData = &addr->sin6_addr;
				copyData.cbData = sizeof(addr->sin6_addr);
				SendMessage(hSenderWnd, WM_COPYDATA, 6, (LPARAM)&copyData);
			}

			break;
		}
	}

	return g_NextProcTable.lpWSPConnect(s, name, namelen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS, lpErrno);
}

int WSPAPI WSPSendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr FAR* lpTo, int iTolen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine, LPWSATHREADID lpThreadId, LPINT lpErrno)
{
	for (int i = 0; i < iCurrentProcNum; i++)
	{
		if (lstrcmp(g_szCurrentApp, g_szHookProc[i]) == 0)
		{
			if (iTolen == sizeof(sockaddr_in)) {
				sockaddr_in* addr = (sockaddr_in*)lpTo;
				TCHAR cAddr[16] = { 0 };
				InetNtop(AF_INET, &addr->sin_addr, cAddr, 16);
				PrintDebugString(true, _T("%s.sendto[%s:%d]"), g_szCurrentApp, cAddr, ntohs(addr->sin_port));

				COPYDATASTRUCT copyData;
				copyData.lpData = &addr->sin_addr;
				copyData.cbData = sizeof(addr->sin_addr);
				if (0 != SendMessage(hSenderWnd, WM_COPYDATA, 4, (LPARAM)&copyData)) {
					PrintDebugString(false, _T("SendMessage(%x)：%s"), hSenderWnd, ErrWrap{}().c_str());
				}

			}
			else if (iTolen == sizeof(sockaddr_in6)) {
				sockaddr_in6* addr = (sockaddr_in6*)lpTo;
				TCHAR cAddr[46] = { 0 };
				InetNtop(AF_INET6, &addr->sin6_addr, cAddr, 46);
				PrintDebugString(true, _T("%s.sendto[%s:%d]"), g_szCurrentApp, cAddr, ntohs(addr->sin6_port));

				COPYDATASTRUCT copyData;
				copyData.lpData = &addr->sin6_addr;
				copyData.cbData = sizeof(addr->sin6_addr);
				if (0 != SendMessage(hSenderWnd, WM_COPYDATA, 6, (LPARAM)&copyData)) {
					PrintDebugString(false, _T("SendMessage(%x)：%s"), hSenderWnd, ErrWrap{}().c_str());
				}
			}

			break;
		}
	}

	return g_NextProcTable.lpWSPSendTo(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags,
		lpTo, iTolen, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
}

_Must_inspect_result_ int WSPAPI WSPStartup(
	_In_ WORD wVersionRequested,
	_In_ LPWSPDATA lpWSPData,
	_In_ LPWSAPROTOCOL_INFOW lpProtocolInfo,
	_In_ WSPUPCALLTABLE UpcallTable,
	_Out_ LPWSPPROC_TABLE lpProcTable
)
{	
	if (lpProtocolInfo->ProtocolChain.ChainLen <= 1)
	{
		return WSAEPROVIDERFAILEDINIT;
	}

	//获取本进程名，不将此操作放入dllmain中是因为尽量延后获取窗口句柄的时机，怕attch dll的时机非常靠前
	if (g_szCurrentApp[0] == _T('\0')) {
		GetModuleFileName(NULL, g_szCurrentApp, MAX_PATH);
		PathStripPath(g_szCurrentApp);
		PathRemoveExtension(g_szCurrentApp);
		PrintDebugString(true, _T("%s.%s, szProtocol:%s"), g_szCurrentApp, __FUNC__, lpProtocolInfo->szProtocol);
		if (lstrcmp(g_szCurrentApp, _T("sender")) == 0) {
			//sender的窗口必须在网络操作之前创建好，且属于同一线程
			EnumThreadWindows(GetCurrentThreadId(), EnumThreadWndProc, NULL);
		}
	}

	// 枚举协议，找到下层协议的WSAPROTOCOL_INFOW结构
	WSAPROTOCOL_INFOW NextProtocolInfo;
	int nTotalProtos;
	LPWSAPROTOCOL_INFOW pProtoInfo = GetProvider(&nTotalProtos);
	if (pProtoInfo == NULL) {
		return WSAEPROVIDERFAILEDINIT;
	}
	// 下层入口ID
	DWORD dwBaseEntryId = lpProtocolInfo->ProtocolChain.ChainEntries[1];
	bool findNext = false;
	for (int i = 0; i < nTotalProtos; i++)
	{
		if (pProtoInfo[i].dwCatalogEntryId == dwBaseEntryId)
		{
			findNext = true;
			memcpy(&NextProtocolInfo, &pProtoInfo[i], sizeof(NextProtocolInfo));
			break;
		}
	}
	if (!findNext)
	{
		FreeProvider(pProtoInfo);
		PrintDebugString(false, _T("Can not find underlying protocol"));
		return WSAEPROVIDERFAILEDINIT;
	}
	// 加载下层协议的DLL
	WCHAR szBaseProviderDll[MAX_PATH] = { 0 };
	int nLen = MAX_PATH;
	int nError;
	// 取得下层提供程序DLL路径
	if (WSCGetProviderPath(&NextProtocolInfo.ProviderId, szBaseProviderDll, &nLen, &nError) == SOCKET_ERROR)
	{
		FreeProvider(pProtoInfo);
		PrintDebugString(false, _T("%s: WSCGetProviderPath() failed: %s"), __FUNC__, ErrWrap{}(nError).c_str());
		return WSAEPROVIDERFAILEDINIT;
	}
	TCHAR szExpandProviderDll[MAX_PATH] = { 0 };
	if (!ExpandEnvironmentStrings(szBaseProviderDll, szExpandProviderDll, MAX_PATH))
	{
		FreeProvider(pProtoInfo);
		PrintDebugString(false, _T("%s: ExpandEnvironmentStrings() failed: %s"), __FUNC__, ErrWrap{}().c_str());
		return WSAEPROVIDERFAILEDINIT;
	}
	// 加载下层提供程序
	HMODULE hModule = LoadLibrary(szExpandProviderDll);
	if (hModule == NULL)
	{
		FreeProvider(pProtoInfo);
		PrintDebugString(false, _T("%s: LoadLibrary() failed %s"), __FUNC__, ErrWrap{}().c_str());
		return WSAEPROVIDERFAILEDINIT;
	}
	// 导入下层提供程序的WSPStartup函数
	LPWSPSTARTUP pfnWSPStartup = (LPWSPSTARTUP)GetProcAddress(hModule, "WSPStartup");
	if (pfnWSPStartup == NULL)
	{
		FreeProvider(pProtoInfo);
		PrintDebugString(false, _T("%s: GetProcAddress() failed %s"), __FUNC__, ErrWrap{}().c_str());
		return WSAEPROVIDERFAILEDINIT;
	}
	// 调用下层提供程序的WSPStartup函数
	LPWSAPROTOCOL_INFOW pInfo = lpProtocolInfo;
	if (NextProtocolInfo.ProtocolChain.ChainLen == BASE_PROTOCOL) {
		pInfo = &NextProtocolInfo;
	}
	int nRet = pfnWSPStartup(wVersionRequested, lpWSPData, pInfo, UpcallTable, lpProcTable);
	if (nRet != ERROR_SUCCESS)
	{
		FreeProvider(pProtoInfo);
		PrintDebugString(false, _T("%s: underlying provider's WSPStartup() failed %s"), __FUNC__, ErrWrap{}(nRet).c_str());
		return nRet;
	}

	// 保存下层提供者的函数表
	g_NextProcTable = *lpProcTable;

	// 传给上层，截获对以下函数的调用
	lpProcTable->lpWSPConnect = WSPConnect;
	lpProcTable->lpWSPSendTo = WSPSendTo;

	FreeProvider(pProtoInfo);
	return nRet;

//CleanUp:
//	FreeProvider(pProtoInfo);
//	return WSAEPROVIDERFAILEDINIT;
}
