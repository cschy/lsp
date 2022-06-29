#pragma once

#include <Windows.h>
#include <tchar.h>
#include <string>

//自定义宏
#ifdef UNICODE
#define __FUNC__ __FUNCTIONW__
#define tstring std::wstring
#define to_tstring std::to_wstring
#define tifstream std::wifstream
#define tstringstream std::wstringstream
#else
#define __FUNC__ __FUNCTION__
#define tstring std::string
#define to_tstring std::to_string
#define tifstream std::ifstream
#define tstringstream std::stringstream
#endif // UNICODE

void PrintDebugString(bool flag, LPCTSTR lpszFmt, ...)
{
#define FLAG_YES _T("Success: ")
#define FLAG_BAD _T("Failure: ")
	const tstring strFlag{ flag ? FLAG_YES : FLAG_BAD };

	va_list args;
	va_start(args, lpszFmt);
	int len = _vsctprintf(lpszFmt, args) + 2 + strFlag.length();
	TCHAR* lpszBuf = (TCHAR*)_malloca(len * sizeof(TCHAR));//栈中分配, 不需要释放
	if (lpszBuf == NULL) {
		_tprintf_s((tstring(FLAG_BAD) + _T("_malloca lpszBuf NULL\n")).c_str());
		OutputDebugString((tstring(FLAG_BAD) + _T("_malloca lpszBuf NULL\n")).c_str());
		return;
	}
	lstrcpy(lpszBuf, strFlag.c_str());
	_vstprintf_s(lpszBuf + strFlag.length(), len - strFlag.length(), lpszFmt, args);
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
#undef FLAG_BAD
#undef FLAG_YES
}

void PrintDebugStringA(bool flag, LPCSTR lpszFmt, ...)
{
#define FLAG_YES "Success: "
#define FLAG_BAD "Failure: "
	const std::string strFlag{ flag ? FLAG_YES : FLAG_BAD };

	va_list args;
	va_start(args, lpszFmt);
	int len = _vscprintf(lpszFmt, args) + 2 + strFlag.length();
	char* lpszBuf = (char*)_malloca(len * sizeof(char));//栈中分配, 不需要释放
	if (lpszBuf == NULL) {
		printf((std::string(FLAG_BAD) + "_malloca lpszBuf NULL\n").c_str());
		OutputDebugStringA((std::string(FLAG_BAD) + "_malloca lpszBuf NULL\n").c_str());
		return;
	}
	lstrcpyA(lpszBuf, strFlag.c_str());
	vsprintf_s(lpszBuf + strFlag.length(), len - strFlag.length(), lpszFmt, args);
	va_end(args);
	lpszBuf[len - 2] = '\n';
	lpszBuf[len - 1] = '\0';

#ifdef _CONSOLE
	if (IsWindowVisible(GetConsoleWindow())) {
		printf(lpszBuf);
	}
	else {
		OutputDebugStringA(lpszBuf);
	}
#else
	OutputDebugStringA(lpszBuf);
#endif // _CONSOLE
#undef FLAG_BAD
#undef FLAG_YES
}

class ErrWrap {
private:
	TCHAR* szMsgBuf;
public:
	~ErrWrap() {
		LocalFree(szMsgBuf);
	}
	tstring operator()(int num = GetLastError()) {
		tstring ret = to_tstring(num) + _T(":");
		if (FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			num,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&szMsgBuf,
			0, NULL)) {
			ret.append(szMsgBuf);
		}
		else {
			TCHAR szFormatError[32];
			_stprintf_s(szFormatError, _T("FormatMessage Error:%d"), GetLastError());
			ret.append(szFormatError);
		}
		return ret;
	}
};

class RegWrap {
private:
	HKEY hKey;
public:
	RegWrap() = default;
	RegWrap(HKEY hKeyRoot, const TCHAR* szDir) {
		LSTATUS ret = RegOpenKeyEx(hKeyRoot, szDir, 0, KEY_ALL_ACCESS, &hKey);
		if (ret != ERROR_SUCCESS) {
			PrintDebugString(false, _T("RegOpenKeyEx(%x,%s):%s"), hKeyRoot, szDir, ErrWrap{}(ret).c_str());
		}
		PrintDebugString(true, _T("this(%p)->RegOpenKeyEx(%x,%s)"), this, hKeyRoot, szDir);
	}
	~RegWrap() {
		RegCloseKey(hKey);
	}
	bool open(HKEY hKeyRoot, const TCHAR* szDir) {
		LSTATUS ret = RegOpenKeyEx(hKeyRoot, szDir, 0, KEY_ALL_ACCESS, &hKey);
		if (ret != ERROR_SUCCESS) {
			PrintDebugString(false, _T("RegOpenKeyEx(%x,%s):%s"), hKeyRoot, szDir, ErrWrap{}(ret).c_str());
			return false;
			//#define HKEY_CLASSES_ROOT                   (( HKEY ) (ULONG_PTR)((LONG)0x80000000) )
			//#define HKEY_CURRENT_USER                   (( HKEY ) (ULONG_PTR)((LONG)0x80000001) )
			//#define HKEY_LOCAL_MACHINE                  (( HKEY ) (ULONG_PTR)((LONG)0x80000002) )
			//#define HKEY_USERS                          (( HKEY ) (ULONG_PTR)((LONG)0x80000003) )
		}
		PrintDebugString(true, _T("this(%p)->RegOpenKeyEx(%x,%s)"), this, hKeyRoot, szDir);
		return true;
	}
	bool set(const TCHAR* key, DWORD dwType, const PVOID value, DWORD cbData) {
		LSTATUS ret = RegSetValueEx(hKey, key, 0, dwType, (BYTE*)value, cbData);
		if (ret != ERROR_SUCCESS) {
			PrintDebugString(false, _T("this(%p)->RegSetValueEx(%s,%d,%d):%s"), this, key, dwType, cbData, ErrWrap{}(ret).c_str());
			//#define REG_NONE                    ( 0ul ) // No value type
			//#define REG_SZ                      ( 1ul ) // Unicode nul terminated string
			//#define REG_EXPAND_SZ               ( 2ul ) // Unicode nul terminated string
			//			// (with environment variable references)
			//#define REG_BINARY                  ( 3ul ) // Free form binary
			//#define REG_DWORD                   ( 4ul ) // 32-bit number
			//#define REG_DWORD_LITTLE_ENDIAN     ( 4ul ) // 32-bit number (same as REG_DWORD)
			//#define REG_DWORD_BIG_ENDIAN        ( 5ul ) // 32-bit number
			//#define REG_LINK                    ( 6ul ) // Symbolic Link (unicode)
			//#define REG_MULTI_SZ                ( 7ul ) // Multiple Unicode strings
			//#define REG_RESOURCE_LIST           ( 8ul ) // Resource list in the resource map
			//#define REG_FULL_RESOURCE_DESCRIPTOR ( 9ul ) // Resource list in the hardware description
			//#define REG_RESOURCE_REQUIREMENTS_LIST ( 10ul )
			//#define REG_QWORD                   ( 11ul ) // 64-bit number
			//#define REG_QWORD_LITTLE_ENDIAN     ( 11ul ) // 64-bit number (same as REG_QWORD)
			return false;
		}
		return true;
	}
	bool get(const TCHAR* key, LPVOID value, DWORD dwSize) {
		LSTATUS ret = RegQueryValueEx(hKey, key, NULL, NULL, (LPBYTE)value, &dwSize);
		if (ret != ERROR_SUCCESS) {
			PrintDebugString(false, _T("this(%p)->RegQueryValueEx(%s,%d):%s"), this, key, dwSize, ErrWrap{}(ret).c_str());
			return false;
		}
		return true;
	}
	bool del(const TCHAR* key) {
		LSTATUS ret = RegDeleteValue(hKey, key);
		if (ret != ERROR_SUCCESS) {
			PrintDebugString(false, _T("this(%p)->RegDeleteValue(%s):%s"), this, key, ErrWrap{}(ret).c_str());
			return false;
		}
		return true;
	}
};