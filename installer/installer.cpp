// installer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
//#include <afxwin.h>
#include <iostream>
#include <Ws2spi.h>
#include <tchar.h>
#include <SpOrder.h>
#include <string>
#include <vector>
#include <ShlObj.h>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")


#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll

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

#define LSP_ENTRYID _T("LSP-CatalogEntryId")
#define LSP_ENTRYID_LEN 32

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

LPWSAPROTOCOL_INFOW GetProvider(LPINT lpTotalProtocols)
{
    DWORD dwSize = 0;
    int nError;
    LPWSAPROTOCOL_INFOW pProtoInfo = NULL;

    // 取得需要的长度
    if (WSCEnumProtocols(NULL, pProtoInfo, &dwSize, &nError) == SOCKET_ERROR)
    {
        if (nError != WSAENOBUFS) {
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

bool InstallProvider(const WCHAR* wszDllPath)   //参数：LSP的DLL的地址
{
    WCHAR wszLSPName[] = L"MyLSP";
    WSAPROTOCOL_INFOW OriginalProtocolInfo[4];   //数组成员为TCP、UDP、TCP6、UDP6
    int nArrayCount = 0;    //数组个数索引
    int nError;

    int nProtocols = 0;
    LPWSAPROTOCOL_INFOW pProtoInfo = GetProvider(&nProtocols);
    if (nProtocols < 1 || pProtoInfo == NULL) {
        return false;
    }

    bool bFindUdp = false, bFindTcp = false, bFindUdp6 = false, bFindTcp6 = false;
    for (int i = 0; i < nProtocols; i++)
    {
        if (pProtoInfo[i].iAddressFamily == AF_INET)
        {
            if (!bFindTcp && pProtoInfo[i].iProtocol == IPPROTO_TCP)
            {
                bFindTcp = true;
                memcpy(&OriginalProtocolInfo[nArrayCount], &pProtoInfo[i], sizeof(WSAPROTOCOL_INFOW));
                //去除XP1_IFS_HANDLES标志,防止提供者返回的句柄是真正的操作系统句柄
                OriginalProtocolInfo[nArrayCount].dwServiceFlags1 &= (~XP1_IFS_HANDLES);
                nArrayCount++;
            }
            if (!bFindUdp && pProtoInfo[i].iProtocol == IPPROTO_UDP)
            {
                bFindUdp = true;
                memcpy(&OriginalProtocolInfo[nArrayCount], &pProtoInfo[i], sizeof(WSAPROTOCOL_INFOW));
                //去除XP1_IFS_HANDLES标志,防止提供者返回的句柄是真正的操作系统句柄
                OriginalProtocolInfo[nArrayCount].dwServiceFlags1 &= (~XP1_IFS_HANDLES);
                nArrayCount++;
            }
        }
        if (pProtoInfo[i].iAddressFamily == AF_INET6)
        {
            if (!bFindTcp6 && pProtoInfo[i].iProtocol == IPPROTO_TCP)
            {
                bFindTcp6 = true;
                memcpy(&OriginalProtocolInfo[nArrayCount], &pProtoInfo[i], sizeof(WSAPROTOCOL_INFOW));
                //去除XP1_IFS_HANDLES标志,防止提供者返回的句柄是真正的操作系统句柄
                OriginalProtocolInfo[nArrayCount].dwServiceFlags1 &= (~XP1_IFS_HANDLES);
                nArrayCount++;
            }
            if (!bFindUdp6 && pProtoInfo[i].iProtocol == IPPROTO_UDP)
            {
                bFindUdp6 = true;
                memcpy(&OriginalProtocolInfo[nArrayCount], &pProtoInfo[i], sizeof(WSAPROTOCOL_INFOW));
                //去除XP1_IFS_HANDLES标志,防止提供者返回的句柄是真正的操作系统句柄
                OriginalProtocolInfo[nArrayCount].dwServiceFlags1 &= (~XP1_IFS_HANDLES);
                nArrayCount++;
            }
        }
    }
    if (nArrayCount == 0)
    {
        FreeProvider(pProtoInfo);
        return false;
    }
    //安装LSP分层协议
    WSAPROTOCOL_INFOW LayeredProtocolInfo;
    memcpy(&LayeredProtocolInfo, &OriginalProtocolInfo[0], sizeof(WSAPROTOCOL_INFOW));
    //修改协议名称的字符串
    lstrcpy(LayeredProtocolInfo.szProtocol, wszLSPName);
    //表示分层协议
    LayeredProtocolInfo.ProtocolChain.ChainLen = LAYERED_PROTOCOL;//0
    //表示方式为由提供者自己设置
    LayeredProtocolInfo.dwProviderFlags = PFL_HIDDEN;
    //安装分层协议
    GUID Layered_guid;
    CoCreateGuid(&Layered_guid);
    if (SOCKET_ERROR == WSCInstallProvider(&Layered_guid, wszDllPath, &LayeredProtocolInfo, 1, &nError))
    {
        _tprintf_s(_T("安装分层协议失败：%d\n"), nError);
        FreeProvider(pProtoInfo);
        return false;
    }
    FreeProvider(pProtoInfo);


    //重新遍历协议,获取分层协议的目录ID号
    DWORD dwLayeredCatalogId = 0;   //分层协议的入口ID号
    pProtoInfo = GetProvider(&nProtocols);
    if (nProtocols < 1 || pProtoInfo == NULL) {
        return false;
    }
    for (int i = 0; i < nProtocols; i++)    //一般安装新入口后,会排在最低部
    {
        if (memcmp(&pProtoInfo[i].ProviderId, &Layered_guid, sizeof(GUID)) == 0)
        {
            //取出分层协议的目录入口ID
            dwLayeredCatalogId = pProtoInfo[i].dwCatalogEntryId;
            
            TCHAR v[LSP_ENTRYID_LEN] = { 0 };
            _stprintf_s(v, _T("%d"), dwLayeredCatalogId);
            _tprintf_s(_T("分层协议的目录入口ID：%s\n"), v);
            _AddEnv(LSP_ENTRYID, v);
            //取环境变量测试
            TCHAR test[LSP_ENTRYID_LEN] = { 0 };
            _GetEnv(LSP_ENTRYID, test, LSP_ENTRYID_LEN);
            if (lstrcmp(v, test) == 0) {
                _tprintf_s(_T("写入环境变量成功 [key:%s, value:%s]\n"), LSP_ENTRYID, test);
            }
            break;
        }
    }


    //安装协议链
    WCHAR wszChainName[WSAPROTOCOL_LEN + 1];//新分层协议的名称 over 取出来的入口模板的名称
    for (int i = 0; i < nArrayCount; i++)
    {
        _stprintf_s(wszChainName, _T("%s over %s"), wszLSPName, OriginalProtocolInfo[i].szProtocol);
        ZeroMemory(OriginalProtocolInfo[i].szProtocol, sizeof(OriginalProtocolInfo[i].szProtocol));
        lstrcpy(OriginalProtocolInfo[i].szProtocol, wszChainName);  //将这个模板的名称改成新名称↑
        if (OriginalProtocolInfo[i].ProtocolChain.ChainLen == 1)    //这是基础协议的模板
        {   //修改基础协议模板的协议链, 在协议链[1]写入真正TCP/UDP[基础协议]的入口ID
            OriginalProtocolInfo[i].ProtocolChain.ChainEntries[1] = OriginalProtocolInfo[i].dwCatalogEntryId;
        }
        else if (OriginalProtocolInfo[i].ProtocolChain.ChainLen > 1)
        {   //如果大于1,相当于是个协议链,表示：将协议链中的入口ID,全部向后退一格,留出[0]
            for (int j = OriginalProtocolInfo[i].ProtocolChain.ChainLen; j > 0; j--)
                OriginalProtocolInfo[i].ProtocolChain.ChainEntries[j] = OriginalProtocolInfo[i].ProtocolChain.ChainEntries[j - 1];
        }
        //让新分层协议排在基础协议的前面
        OriginalProtocolInfo[i].ProtocolChain.ChainLen++;
        OriginalProtocolInfo[i].ProtocolChain.ChainEntries[0] = dwLayeredCatalogId;
    }
    //一次安装4个协议链
    GUID AgreementChain_guid;
    CoCreateGuid(&AgreementChain_guid);
    if (SOCKET_ERROR == WSCInstallProvider(&AgreementChain_guid, wszDllPath, OriginalProtocolInfo, nArrayCount, &nError))
    {
        _tprintf_s(_T("安装协议链失败：%d\n"), nError);
        FreeProvider(pProtoInfo);
        return false;
    }
    FreeProvider(pProtoInfo);


    //第三步：将所有协议进行重新排序，让我们的协议链排在最前面
    //重新遍历所有协议
    pProtoInfo = GetProvider(&nProtocols);
    if (nProtocols < 1 || pProtoInfo == NULL) {
        return false;
    }
    DWORD dwIds[128];
    int nIndex = 0;
    //添加我们的协议链
    for (int i = 0; i < nProtocols; i++)
    {   //如果是我们新创建的协议链
        if (pProtoInfo[i].ProtocolChain.ChainLen > 1 && pProtoInfo[i].ProtocolChain.ChainEntries[0] == dwLayeredCatalogId)
            dwIds[nIndex++] = pProtoInfo[i].dwCatalogEntryId;//将我们的协议链排最前面
    }
    //添加其他协议
    for (int i = 0; i < nProtocols; i++)
    {   //如果是基础协议,分层协议(不包括我们的协议链,但包括我们的分层协议)
        if (pProtoInfo[i].ProtocolChain.ChainLen <= 1 || pProtoInfo[i].ProtocolChain.ChainEntries[0] != dwLayeredCatalogId)
            dwIds[nIndex++] = pProtoInfo[i].dwCatalogEntryId;
    }
    //重新排序Winsock目录
    if (WSCWriteProviderOrder(dwIds, nIndex) != ERROR_SUCCESS) {
        FreeProvider(pProtoInfo);
        return false;
    }

    FreeProvider(pProtoInfo);

    return true;
}

bool RemoveProvider(DWORD dwLayeredCatalogId)
{
    _tprintf_s(_T("分层协议入口ID：%d\n"), dwLayeredCatalogId);
    LPWSAPROTOCOL_INFOW pProtoInfo = NULL;
    int nProtocols = 0;
    //遍历出所有协议
    pProtoInfo = GetProvider(&nProtocols);
    if (nProtocols < 1 || pProtoInfo == NULL) {
        return false;
    }
    
    int i = 0;
    GUID Layered_guid;
    for (i = 0; i < nProtocols; i++)
    {   //查找分层协议提供者
        if (dwLayeredCatalogId == pProtoInfo[i].dwCatalogEntryId)
        {
            Layered_guid = pProtoInfo[i].ProviderId;
            break;
        }
    }
    if (i == nProtocols) {
        _tprintf_s(_T("没有找到相应的分层协议\n"));
        return false;
    }
    
    int nError = 0;
    for (i = 0; i < nProtocols; i++)
    {   //查找协议链(这个协议链的[0]为分层协议提供者)
        if (pProtoInfo[i].ProtocolChain.ChainLen > 1 && pProtoInfo[i].ProtocolChain.ChainEntries[0] == dwLayeredCatalogId)
        {   //先卸载协议链
            if (SOCKET_ERROR == WSCDeinstallProvider(&pProtoInfo[i].ProviderId, &nError))
            {
                _tprintf_s(_T("卸载协议链失败：%d\n"), nError);
                return false;
            }
            break;
        }
    }
    if (SOCKET_ERROR == WSCDeinstallProvider(&Layered_guid, &nError))
    {
        _tprintf_s(_T("卸载分层协议失败：%d\n"), nError);
        return false;
    }
    return true;
}

//void GetPathExceptSelf(std::vector<tstring>& v)
//{
//    TCHAR szSelfPath[MAX_PATH];
//    GetModuleFileName(NULL, szSelfPath, MAX_PATH);
//
//    CFileFind finder;
//    BOOL bWorking = finder.FindFile(_T("*.*"));
//    while (bWorking)
//    {
//        bWorking = finder.FindNextFile();
//        if (finder.IsDots() || finder.IsDirectory() || finder.GetFilePath() == CString(szSelfPath)) {
//            continue;
//        }
//        v.emplace_back(finder.GetFilePath());
//    }
//}

bool GetUserSelectDir(tstring& res)
{
    HRESULT hr = CoInitialize(NULL);
    DWORD dwFlags;
    IFileOpenDialog* pfd = NULL;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    hr = pfd->GetOptions(&dwFlags);
    hr = pfd->SetOptions(dwFlags | FOS_PICKFOLDERS);
    IShellItem* pShItem = NULL;
    SHCreateItemFromParsingName(L"C:\\Program Files (x86)", NULL, IID_PPV_ARGS(&pShItem));
    pfd->SetDefaultFolder(pShItem);
    pfd->SetTitle(L"请选择存放dll的文件夹");
    hr = pfd->Show(GetConsoleWindow());

    if (hr == S_OK)
    {
        IShellItem* pItem = NULL;
        if (S_OK == pfd->GetResult(&pItem)) {
            LPWSTR lpSelect = NULL;
            HRESULT ret = pItem->GetDisplayName(SIGDN_FILESYSPATH, &lpSelect);
            if (ret == S_OK) {
                res = lpSelect;
                CoTaskMemFree(lpSelect);//free memory;
                CoUninitialize();
                return true;
            }
        }
    }
    CoUninitialize();
    return false;
}

int _tmain(int argc, TCHAR* argv[])
{
    setlocale(LOCALE_ALL, "chs");

    int ans = MessageBox(NULL, _T("安装：是，卸载：否，什么也不做：取消"), _T("提示"), MB_YESNOCANCEL | MB_DEFBUTTON3 | MB_ICONQUESTION);
    if (ans == IDCANCEL) {
        return 0;
    }
    else if (ans == IDNO) {
        TCHAR szEntryId[LSP_ENTRYID_LEN];
        if (!_GetEnv(LSP_ENTRYID, szEntryId, LSP_ENTRYID_LEN)) {
            MessageBox(NULL, _T("已经卸载"), _T("提示"), MB_ICONINFORMATION);
            return 0;
        }
        _tprintf_s(_T("获取环境变量成功 [key: %s, value:%s]\n"), LSP_ENTRYID, szEntryId);
        DWORD dwEntryId;
        _stscanf_s(szEntryId, _T("%d"), &dwEntryId);
        bool res = RemoveProvider(dwEntryId);
        if (res) {
            if (_DelEnv(LSP_ENTRYID)) {
                _tprintf_s(_T("删除环境变量成功 [key:%s]\n"), LSP_ENTRYID);
            }
            MessageBox(NULL, _T("卸载分层协议及其协议链成功，为保证正常的网络环境，\n请注销或重启"), _T("提示"), MB_ICONINFORMATION);
        }
        else {
            MessageBox(NULL, _T("卸载分层协议及其协议链失败"), _T("错误"), MB_ICONWARNING);
        }
        return 0;
    }

    _tprintf_s(_T("第一步：检查环境变量判断是否已经安装：%s\n"), LSP_ENTRYID);
    TCHAR szEntryId[LSP_ENTRYID_LEN];
    if (_GetEnv(LSP_ENTRYID, szEntryId, LSP_ENTRYID_LEN)) {
        int ans = MessageBox(NULL, _T("检测到已经安装，是否重新安装？"), _T("提示"), MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONQUESTION);
        if (ans == IDCANCEL) {
            return 0;
        }
    }

    /*tstring selectDir;
    if (!GetUserSelectDir(selectDir)) {
        MessageBox(GetConsoleWindow(), _T("请选择存放dll的文件夹，最好放在系统盘的目录下"), _T("提示"), MB_OK);
        return 0;
    }
    _tprintf_s(_T("选择安放dll的文件夹：%s\n"), selectDir.c_str());*/

    const TCHAR szDllFile[] = _T("lsp.dll");
    const TCHAR szCfgFile[] = _T("lsp.config");
    const TCHAR szSelectDir[] = _T("C:\\Program Files (x86)\\OneClickClientService\\SystemProtect\\lsp");
    _tprintf_s(_T("第二步：拷贝dll及其资源到安全目录：%s\n"), szSelectDir);
    if (!PathIsDirectory(szSelectDir)) {
        if (!CreateDirectory(szSelectDir, NULL) && GetLastError() == ERROR_PATH_NOT_FOUND) {
            MessageBox(NULL, _T("非统一客户端用户禁止使用"), _T("错误"), MB_ICONWARNING);
            return 0;
        }
    }

    TCHAR srcDllFile[MAX_PATH], srcCfgFile[MAX_PATH];
    GetModuleFileName(NULL, srcDllFile, MAX_PATH);
    PathRemoveFileSpec(srcDllFile);
    lstrcpy(srcCfgFile, srcDllFile);

    PathAppend(srcDllFile, szDllFile);
    PathAppend(srcCfgFile, szCfgFile);

    TCHAR dstDllFile[MAX_PATH];
    lstrcpy(dstDllFile, szSelectDir);
    PathAppend(dstDllFile, szDllFile);

    TCHAR dstCfgFile[MAX_PATH];
    lstrcpy(dstCfgFile, szSelectDir);
    PathAppend(dstCfgFile, szCfgFile);

    if (!CopyFile(srcDllFile, dstDllFile, FALSE)) {
        _tprintf_s(_T("CopyFile失败：%d[src:%s, dst:%s]\n"), GetLastError(), srcDllFile, dstDllFile);
    }
    if (!CopyFile(srcCfgFile, dstCfgFile, FALSE)) {
        _tprintf_s(_T("CopyFile失败：%d[src:%s, dst:%s]\n"), GetLastError(), srcCfgFile, dstCfgFile);
    }
    /*std::vector<tstring> paths;
    GetPathExceptSelf(paths);
    for (auto& i : paths) {
        TCHAR srcFileName[MAX_PATH];
        lstrcpy(srcFileName, i.c_str());
        PathStripPath(srcFileName);
        _tprintf_s(_T("源文件：%s\n"), srcFileName);

        TCHAR dstFileName[MAX_PATH];
        lstrcpy(dstFileName, selectDir.c_str());
        PathAppend(dstFileName, srcFileName);
        _tprintf_s(_T("目的文件：%s\n"), dstFileName);
        if (CopyFile(i.c_str(), dstFileName, FALSE)) {
            _tprintf_s(_T("拷贝成功\n\n"));
        }
        else {
            _tprintf_s(_T("拷贝失败：%d\n\n"), GetLastError());
        }
    }*/
    

    _tprintf_s(_T("第三步：安装dll：%s\n"), dstDllFile);
    if (InstallProvider(dstDllFile)) {
        //开启sender进程
        TCHAR szSenderProc[] = _T("sender.exe");
        STARTUPINFO si = { 0 };
        PROCESS_INFORMATION pi = { 0 };
        /*ZeroMemory(&si, sizeof(STARTUPINFO));
        ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));*/
        si.cb = sizeof(STARTUPINFO);
        si.wShowWindow = SW_HIDE;
        si.dwFlags = STARTF_USESHOWWINDOW;
        CreateProcess(NULL, szSenderProc, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
        MessageBox(NULL, _T("安装分层协议及其协议链成功，请重启需要联网的通讯软件"), _T("提示"), MB_ICONINFORMATION);
    }
    else {
        MessageBox(NULL, _T("安装分层协议及其协议链失败"), _T("错误"), MB_ICONWARNING);
    }

    return 0;
}
