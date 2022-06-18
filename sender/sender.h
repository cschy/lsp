#pragma once

#include "resource.h"


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