// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")
#include <WS2spi.h>
#include <tchar.h>
#include <WS2tcpip.h>
#include <string>
#include <fstream>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

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
#endif // UNICODE
#define MAX_PROC_COUNT 16
#define MAX_PROC_NAME 16
#define CONFIG_FILE _T("lsp.config")

TCHAR	g_szDllPath[MAX_PATH];	// 当前DLL路径
TCHAR	g_szCurrentApp[MAX_PATH];	// 当前调用本DLL的程序的名称
WSPPROC_TABLE g_NextProcTable;		// 下层函数列表
TCHAR g_szHookProc[MAX_PROC_COUNT][MAX_PROC_NAME] = { 0 };
unsigned int iCurrentProcNum;

void PrintDebugString(LPCTSTR lpszFmt, ...)
{
	va_list args;
	va_start(args, lpszFmt);
	int len = _vsctprintf(lpszFmt, args) + 2;
	TCHAR* lpszBuf = (TCHAR*)_malloca(len * sizeof(TCHAR));//栈中分配, 不需要释放
	if (lpszBuf == NULL)
	{
		OutputDebugStringA("Failure: _malloca lpszBuf NULL");
		return;
	}
	_vstprintf_s(lpszBuf, len, lpszFmt, args);
	va_end(args);
	lpszBuf[len - 2] = _T('\n');
	lpszBuf[len - 1] = _T('\0');
	OutputDebugString(lpszBuf);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		ZeroMemory(g_szDllPath, sizeof(g_szDllPath));
		GetModuleFileName(hModule, g_szDllPath, MAX_PATH);
		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

LPWSAPROTOCOL_INFOW GetProvider(LPINT lpnTotalProtocols)
{
	LPWSAPROTOCOL_INFOW pProtoInfo = NULL;
	DWORD dwSize = 0;
	int nError;

	// 取得需要的长度
	if (WSCEnumProtocols(NULL, pProtoInfo, &dwSize, &nError) == SOCKET_ERROR)
	{
		if (nError != WSAENOBUFS) {
			return NULL;
		}
	}

	pProtoInfo = (LPWSAPROTOCOL_INFO)GlobalAlloc(GPTR, dwSize);
	*lpnTotalProtocols = WSCEnumProtocols(NULL, pProtoInfo, &dwSize, &nError);
	return pProtoInfo;
}

void FreeProvider(LPWSAPROTOCOL_INFOW pProtoInfo)
{
	GlobalFree(pProtoInfo);
}

int WSPAPI WSPConnect(
	SOCKET s,
	const struct sockaddr FAR* name,
	int namelen,
	LPWSABUF lpCallerData,
	LPWSABUF lpCalleeData,
	LPQOS lpSQOS,
	LPQOS lpGQOS,
	LPINT lpErrno
)
{
	for (int i = 0; i < iCurrentProcNum; i++)
	{
		if (lstrcmp(g_szCurrentApp, g_szHookProc[i]) == 0)
		{
			if (namelen == sizeof(sockaddr_in)) {
				sockaddr_in* addr = (sockaddr_in*)name;
				TCHAR cAddr[17] = { 0 };
				InetNtop(AF_INET, &addr->sin_addr, cAddr, 17);
				PrintDebugString(_T("Success: %s.connect[%s:%d]"), g_szCurrentApp, cAddr, ntohs(addr->sin_port));
			}
			else if (namelen == sizeof(sockaddr_in6)) {
				sockaddr_in6* addr = (sockaddr_in6*)name;
				TCHAR cAddr[47] = { 0 };
				InetNtop(AF_INET6, &addr->sin6_addr, cAddr, 47);
				PrintDebugString(_T("Success: %s.connect[%s:%d]"), g_szCurrentApp, cAddr, ntohs(addr->sin6_port));
			}

			break;
		}
	}

	return g_NextProcTable.lpWSPConnect(s, name, namelen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS, lpErrno);
}

int WSPAPI WSPSendTo(
	SOCKET s,
	LPWSABUF lpBuffers,
	DWORD dwBufferCount,
	LPDWORD lpNumberOfBytesSent,
	DWORD dwFlags,
	const struct sockaddr FAR* lpTo,
	int iTolen,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
	LPWSATHREADID lpThreadId,
	LPINT lpErrno
)
{
	for (int i = 0; i < iCurrentProcNum; i++)
	{
		if (lstrcmp(g_szCurrentApp, g_szHookProc[i]) == 0)
		{
			if (iTolen == sizeof(sockaddr_in)) {
				sockaddr_in* addr = (sockaddr_in*)lpTo;
				TCHAR cAddr[17] = { 0 };
				InetNtop(AF_INET, &addr->sin_addr, cAddr, 17);
				PrintDebugString(_T("Success: %s.sendto[%s:%d]"), g_szCurrentApp, cAddr, ntohs(addr->sin_port));
			}
			else if (iTolen == sizeof(sockaddr_in6)) {
				sockaddr_in6* addr = (sockaddr_in6*)lpTo;
				TCHAR cAddr[47] = { 0 };
				InetNtop(AF_INET6, &addr->sin6_addr, cAddr, 47);
				PrintDebugString(_T("Success: %s.sendto[%s:%d]"), g_szCurrentApp, cAddr, ntohs(addr->sin6_port));
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
	ZeroMemory(g_szCurrentApp, sizeof(g_szCurrentApp));
	GetModuleFileName(NULL, g_szCurrentApp, MAX_PATH);
	PathStripPath(g_szCurrentApp);
	PathRemoveExtension(g_szCurrentApp);
	PrintDebugString(_T("Success: %s.%s"), g_szCurrentApp, __FUNC__);
	
	// 从文件加载需要拦截的应用程序
	PathRemoveFileSpec(g_szDllPath);
	PathAppend(g_szDllPath, CONFIG_FILE);
	PrintDebugString(_T("Success: cfgFilePath: %s"), g_szDllPath);
	ifstream ifs(g_szDllPath);
	ZeroMemory(g_szHookProc, sizeof(g_szHookProc));
	int i = 0;
	while (ifs.getline(g_szHookProc[i], MAX_PROC_NAME)) {
		PrintDebugString(_T("Success: 拦截进程：%s"), g_szHookProc[i]);
		i++;
		if (i >= MAX_PROC_COUNT) {
			break;
		}
	}
	iCurrentProcNum = i;
	ifs.close();

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
		PrintDebugString(_T("Failure: %s: Can not find underlying protocol"), __FUNC__);
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
		PrintDebugString(_T("Failure: %s: WSCGetProviderPath() failed %d"), __FUNC__, nError);
		return WSAEPROVIDERFAILEDINIT;
	}
	TCHAR szExpandProviderDll[MAX_PATH] = { 0 };
	if (!ExpandEnvironmentStrings(szBaseProviderDll, szExpandProviderDll, MAX_PATH))
	{
		FreeProvider(pProtoInfo);
		PrintDebugString(_T("Failure: %s: ExpandEnvironmentStrings() failed %d"), __FUNC__, GetLastError());
		return WSAEPROVIDERFAILEDINIT;
	}
	// 加载下层提供程序
	HMODULE hModule = LoadLibrary(szExpandProviderDll);
	if (hModule == NULL)
	{
		FreeProvider(pProtoInfo);
		PrintDebugString(_T("Failure: %s: LoadLibrary() failed %d"), __FUNC__, GetLastError());
		return WSAEPROVIDERFAILEDINIT;
	}
	// 导入下层提供程序的WSPStartup函数
	LPWSPSTARTUP pfnWSPStartup = (LPWSPSTARTUP)GetProcAddress(hModule, "WSPStartup");
	if (pfnWSPStartup == NULL)
	{
		FreeProvider(pProtoInfo);
		PrintDebugString(_T("Failure: %s: GetProcAddress() failed %d"), __FUNC__, GetLastError());
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
		PrintDebugString(_T("Failure: %s: underlying provider's WSPStartup() failed %d"), __FUNC__, nRet);
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
