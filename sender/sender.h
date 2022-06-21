#pragma once

#include "resource.h"
#include <string>
#include <fstream>
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
#endif // UNICODE

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

void PrintDebugString(bool flag, LPCTSTR lpszFmt, ...)
{
	const tstring strGood{ _T("Success: ") };
	const tstring strFail{ _T("Failure: ") };

	va_list args;
	va_start(args, lpszFmt);
	int len = _vsctprintf(lpszFmt, args) + 2 + (flag ? strGood.length() : strFail.length());
	TCHAR* lpszBuf = (TCHAR*)_malloca(len * sizeof(TCHAR));//栈中分配, 不需要释放
	if (lpszBuf == NULL)
	{
		_tprintf_s((strFail + _T("_malloca lpszBuf NULL\n")).c_str());
		OutputDebugString((strFail + _T("_malloca lpszBuf NULL\n")).c_str());
		return;
	}
	if (flag) {
		lstrcpy(lpszBuf, strGood.c_str());
		_vstprintf_s(lpszBuf + strGood.length(), len - strGood.length(), lpszFmt, args);
	}
	else {
		lstrcpy(lpszBuf, strFail.c_str());
		_vstprintf_s(lpszBuf + strFail.length(), len - strFail.length(), lpszFmt, args);
	}
	
	va_end(args);
	lpszBuf[len - 2] = _T('\n');
	lpszBuf[len - 1] = _T('\0');
#ifdef _CONSOLE
	if (IsWindowVisible(GetConsoleWindow())) {
		_tprintf_s(lpszBuf);
	}
	else {
		OutputDebugString(lpszBuf);
	}
#else
	OutputDebugString(lpszBuf);
#endif // _CONSOLE
}

void PrintDebugStringA(bool flag, LPCSTR lpszFmt, ...)
{
#define FLAG_SUCCESSA "Success: "
#define FLAG_FAILUREA "Failure: "
	int success_len = strlen(FLAG_SUCCESSA);
	int failure_len = strlen(FLAG_FAILUREA);

	va_list args;
	va_start(args, lpszFmt);
	int len = _vscprintf(lpszFmt, args) + 2 + (flag ? success_len : failure_len);
	char* lpszBuf = (char*)_malloca(len * sizeof(char));//栈中分配, 不需要释放
	if (lpszBuf == NULL)
	{
		OutputDebugStringA((std::string(FLAG_FAILUREA) + "_malloca lpszBuf NULL").c_str());
		return;
	}
	if (flag) {
		lstrcpyA(lpszBuf, FLAG_SUCCESSA);
		vsprintf_s(lpszBuf + success_len, len - success_len, lpszFmt, args);
	}
	else {
		lstrcpyA(lpszBuf, FLAG_FAILUREA);
		vsprintf_s(lpszBuf + failure_len, len - failure_len, lpszFmt, args);
	}
	va_end(args);
	lpszBuf[len - 2] = _T('\n');
	lpszBuf[len - 1] = _T('\0');
	OutputDebugStringA(lpszBuf);
}

class ErrStr {
public:
	ErrStr() {
		pszError = new TCHAR[MAX_STRLEN];
	}
	~ErrStr() {
		delete[] pszError;
	}
	tstring getStr() {
		_tcserror_s(pszError, MAX_STRLEN, errno);
		return pszError;
	}
private:
	TCHAR* pszError = nullptr;
	static const int MAX_STRLEN = 95;
};