#pragma once

#include <Windows.h>
#include <string>

void print(std::wstring_view str)
{
#ifdef _CONSOLE
	WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), str.data(), str.length(), NULL, NULL);
#else
	OutputDebugStringW(str.data());
#endif
}

std::wstring format(const wchar_t* szFmt, ...)
{
	va_list args;
	va_start(args, szFmt);
	int len = _vscwprintf(szFmt, args) + 1;
	wchar_t* szBuf = (wchar_t*)_malloca(len * sizeof(wchar_t));
	/*if (szBuf == nullptr) [[unlikely]] {
		print(L"false: _malloca error\n");
		return;
	}*/
	vswprintf_s(szBuf, len, szFmt, args);
	va_end(args);
	std::wstring ret{ szBuf };
	_freea(szBuf);
	return ret;
}

consteval size_t getFormatSignCount(std::string_view sv, const char sign = '%') {
	size_t i = 0;
	if (size_t pos = sv.find(sign); pos != std::string_view::npos) {
		if (sv[pos + 1] != sign) [[likely]] {
			i++;
		}
		i += getFormatSignCount(sv.substr(pos + 2), sign);
	}
	return i;
}

#define DbgPrintVar(flag, var) print(format(L###flag": %s\n", var))
#define NUMARGS(...) std::tuple_size_v<decltype(std::make_tuple(__VA_ARGS__))>
#define DbgPrint(flag, fmt, ...) \
static_assert(getFormatSignCount(fmt) == NUMARGS(__VA_ARGS__), \
"the number of format characters should be the same as the number of parameters"); \
print(format(L###flag": "##fmt"\n", ##__VA_ARGS__));

#define _Macro_get_funame(expr)\
	constexpr std::wstring_view __fn_full{ L#expr };\
	constexpr size_t __pos_bracket{ __fn_full.find_first_of(L'(') };\
	static_assert(__pos_bracket != std::wstring_view::npos, "expression does not contain a function");\
	constexpr std::wstring_view __fname{ __fn_full.substr(0, __pos_bracket) };
#define _Macro_dbgprint(fname, code) {\
	constexpr std::wstring_view __file{ std::wstring_view{__FILEW__}.substr(std::wstring_view{__FILEW__}.find_last_of('\\') + 1) };\
	DbgPrint(false, "[%s:%s:%d:%s]: %s", __file.data(), __FUNCTIONW__, __LINE__, fname, ErrorString{ code });\
	static_assert(__file != __FILEW__, "filepath not trimmed");\
}
#define _Macro_error_object(fn, handler) {\
	_Macro_get_funame(fn);\
	auto [res, code] = fn;\
	if (!res) {\
		_Macro_dbgprint(__fname.data(), code);\
		handler;\
	}\
}
#define _Macro_error_native(err_cond, code, handler){\
	_Macro_get_funame(err_cond);\
	constexpr std::string_view __cond{ #err_cond };\
	constexpr size_t __pos_eq{ __cond.find_last_of('=') };\
	static_assert(__pos_eq != std::string_view::npos && (__cond[__pos_eq - 1] == '!' || __cond[__pos_eq - 1] == '='),\
	"this is not a conditional expression");\
	static_assert(__fname.find(L'=') == std::wstring_view::npos, "function should be on the left");\
	if (err_cond) {\
		_Macro_dbgprint(__fname.data(), code);\
		handler;\
	}\
}
#define _Macro_expand(x) x
#define _Macro_fun(_1, _2, _3, name, ...) name 
#define M_ErrHand(...) _Macro_expand(_Macro_fun(__VA_ARGS__, _Macro_error_native, _Macro_error_object)(__VA_ARGS__))

class ErrorString {
public:
	wchar_t* szMsgBuf{ nullptr };//为了格式化打印，只能有这一个数据成员，后续数据成员会占用后面的格式化符号
public:
	ErrorString(std::wstring_view str) {
		szMsgBuf = (wchar_t*)LocalAlloc(LPTR, sizeof(wchar_t) * (str.length() + 1));
		lstrcpyW(szMsgBuf, str.data());
	}
	ErrorString(int num) {
		if (!FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			num,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&szMsgBuf,
			0, NULL))
		{
			auto str{ format(L"Can't FormatMessage %d error: %d", num, GetLastError()) };
			szMsgBuf = (wchar_t*)LocalAlloc(LPTR, sizeof(wchar_t) * (str.length() + 1));
			lstrcpyW(szMsgBuf, str.c_str());
		}
	}
	ErrorString(DWORD num = GetLastError()) {
		if (!FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			num,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&szMsgBuf,
			0, NULL))
		{
			auto str{ format(L"Can't FormatMessage %d error: %d", num, GetLastError()) };
			szMsgBuf = (wchar_t*)LocalAlloc(LPTR, sizeof(wchar_t) * (str.length() + 1));
			lstrcpyW(szMsgBuf, str.c_str());
		}
	}
	~ErrorString() {
		LocalFree(szMsgBuf);
	}
	operator const wchar_t* () {
		return szMsgBuf ? szMsgBuf : L"ErrorString() Error";
	}
	const wchar_t* get_cstr() {
		return szMsgBuf ? szMsgBuf : L"ErrorString() Error";
	}
};

struct Result {
	bool res;
	DWORD code;
	operator bool() {
		return res;
	}
};

class RegWrap {
private:
	HKEY hKey;
public:
	RegWrap() = default;
	RegWrap(HKEY hKeyRoot, const wchar_t* szDir) {
		open(hKeyRoot, szDir);
	}
	~RegWrap() {
		RegCloseKey(hKey);
	}
	
	Result open(HKEY hKeyRoot, const wchar_t* szDir) {
		LSTATUS res{ RegOpenKeyExW(hKeyRoot, szDir, 0, KEY_ALL_ACCESS, &hKey) };
		return { res == ERROR_SUCCESS, (DWORD)res };
	}
	Result set(const wchar_t* key, DWORD dwType, const PVOID value, DWORD cbData) {
		LSTATUS res{ RegSetValueExW(hKey, key, 0, dwType, (BYTE*)value, cbData) };
		return { res == ERROR_SUCCESS, (DWORD)res };
	}
	Result get(const wchar_t* key, LPVOID value, DWORD dwSize) {
		LSTATUS res{ RegQueryValueExW(hKey, key, NULL, NULL, (LPBYTE)value, &dwSize) };
		return { res == ERROR_SUCCESS, (DWORD)res };
	}
	Result del(const wchar_t* key) {
		LSTATUS res{ RegDeleteValueW(hKey, key) };
		return { res == ERROR_SUCCESS, (DWORD)res };
	}
};