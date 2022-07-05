// installer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
//#include <afxwin.h>
#include <fstream>
#include <sstream>

#include <Ws2spi.h>//这个头文件有点刺头，很多东西不能放在它前面
#include <SpOrder.h>
#include <TlHelp32.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ws2_32.lib")
#include "utils.h"


#define PAUSE_RETURN system("pause");return 0
#define REGDIR_ENV _T("Environment")
#define KEY_ENTRYID _T("LSP-CatalogEntryId")
#define FILE_DLL _T("lsp.dll")
#define FILE_DLL_CONFIG _T("lsp.config")
#define FILE_SENDER _T("sender.exe")
#define FILE_SENDER_CONFIG _T("sender.config")
#define DIR_SELECT _T("C:\\Program Files (x86)\\OneClickClientService\\SystemProtect\\lsp\\")
#define EVENT_UNLOADLL _T("Global\\EVENT_UNLOAD_LSPDLL")


RegWrap g_Env;


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
        PrintDebugString(false, _T("安装分层协议失败：%s"), ErrWrap{}(nError).c_str());
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
            
            PrintDebugString(true, _T("分层协议的目录入口ID：%d"), dwLayeredCatalogId);
            g_Env.set(KEY_ENTRYID, REG_DWORD, &dwLayeredCatalogId, sizeof(dwLayeredCatalogId));
            //取环境变量测试
            DWORD dwId;
            g_Env.get(KEY_ENTRYID, &dwId, sizeof(dwId));
            if (dwId == dwLayeredCatalogId) {
                PrintDebugString(true, _T("写入环境变量成功 [key:%s, value:%d]"), KEY_ENTRYID, dwId);
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
        PrintDebugString(false, _T("安装协议链失败：%s"), ErrWrap{}(nError).c_str());
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
    int retWSCWriteProviderOrder = WSCWriteProviderOrder(dwIds, nIndex);
    if (retWSCWriteProviderOrder != ERROR_SUCCESS) {
        PrintDebugString(false, _T("WSCWriteProviderOrder:%s"), ErrWrap{}(retWSCWriteProviderOrder).c_str());
        FreeProvider(pProtoInfo);
        return false;
    }

    FreeProvider(pProtoInfo);

    return true;
}

bool RemoveProvider(DWORD dwLayeredCatalogId)
{
    PrintDebugString(true, _T("分层协议入口ID：%d"), dwLayeredCatalogId);
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
        PrintDebugString(false, _T("没有找到相应的分层协议"));
        return false;
    }
    
    int nError = 0;
    for (i = 0; i < nProtocols; i++)
    {   //查找协议链(这个协议链的[0]为分层协议提供者)
        if (pProtoInfo[i].ProtocolChain.ChainLen > 1 && pProtoInfo[i].ProtocolChain.ChainEntries[0] == dwLayeredCatalogId)
        {   //先卸载协议链
            if (SOCKET_ERROR == WSCDeinstallProvider(&pProtoInfo[i].ProviderId, &nError))
            {
                PrintDebugString(false, _T("卸载协议链失败：%s"), ErrWrap{}(nError).c_str());
                return false;
            }
            break;
        }
    }
    if (SOCKET_ERROR == WSCDeinstallProvider(&Layered_guid, &nError))
    {
        PrintDebugString(false, _T("卸载分层协议失败：%s"), ErrWrap{}(nError).c_str());
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

bool isProcessOpen(const TCHAR* szProc, bool& open)
{
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        PrintDebugString(false, _T("CreateToolhelp32Snapshot: %s"), ErrWrap{}().c_str());
        return false;
    }

    bool find = false;
    BOOL bMore = Process32First(hProcessSnap, &pe32);
    while (bMore) 
    {
        PathRemoveExtension(pe32.szExeFile);
        if (lstrcmp(pe32.szExeFile, szProc) == 0) {
            find = true;
            break;
        }
        bMore = Process32Next(hProcessSnap, &pe32);
    }
    open = find;
    CloseHandle(hProcessSnap);
    return true;
}

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
    if (!g_Env.open(HKEY_CURRENT_USER, REGDIR_ENV)) {
        PAUSE_RETURN;
    }
    DWORD dwEntryId;
    bool bGetEntryId = g_Env.get(KEY_ENTRYID, &dwEntryId, sizeof(dwEntryId));


    PrintDebugString(true, _T("第零步：获取用户请求"));
    int ans = MessageBox(NULL, _T("安装：是，卸载：否，什么也不做：取消"), _T("提示"), MB_YESNOCANCEL | MB_DEFBUTTON3 | MB_ICONQUESTION);
    if (ans == IDCANCEL) {
        PAUSE_RETURN;
    }
    else if (ans == IDNO) {
        if (!bGetEntryId) {
            MessageBox(NULL, _T("已经卸载"), _T("提示"), MB_ICONINFORMATION);
            PAUSE_RETURN;
        }
        if (RemoveProvider(dwEntryId) && g_Env.del(KEY_ENTRYID)) {
            PrintDebugString(true, _T("设置退出lsp.dll事件:%d"), SetEvent(OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_UNLOADLL)));
            MessageBox(NULL, _T("卸载分层协议及其协议链成功"), _T("提示"), MB_ICONINFORMATION);
        }
        else {
            MessageBox(NULL, _T("卸载分层协议及其协议链失败"), _T("错误"), MB_ICONWARNING);
        }
        PAUSE_RETURN;
    }
    

    PrintDebugString(true, _T("第一步：检查环境变量[%s]判断是否已经安装"), KEY_ENTRYID);
    if (bGetEntryId) {
        MessageBox(NULL, _T("检测到已经安装，请先卸载"), _T("提示"), MB_ICONINFORMATION);
        PAUSE_RETURN;
    }
    /*tstring selectDir;
    if (!GetUserSelectDir(selectDir)) {
        MessageBox(GetConsoleWindow(), _T("请选择存放dll的文件夹，最好放在系统盘的目录下"), _T("提示"), MB_OK);
        return 0;
    }
    _tprintf_s(_T("选择安放dll的文件夹：%s\n"), selectDir.c_str());*/


    PrintDebugString(true, _T("第二步：拷贝dll及其资源到安全目录：%s"), DIR_SELECT);
    if (!PathIsDirectory(DIR_SELECT)) {
        if (!CreateDirectory(DIR_SELECT, NULL) && GetLastError() == ERROR_PATH_NOT_FOUND) {
            MessageBox(NULL, _T("非统一客户端用户禁止使用"), _T("错误"), MB_ICONWARNING);
            PAUSE_RETURN;
        }
    }

    auto fnCopyFile = [](const TCHAR* file, const TCHAR* dstFilePath) {
        TCHAR srcFilePath[MAX_PATH];
        GetModuleFileName(NULL, srcFilePath, MAX_PATH);
        PathRemoveFileSpec(srcFilePath);
        PathAppend(srcFilePath, file);

        if (!CopyFile(srcFilePath, dstFilePath, FALSE)) {
            _tprintf_s(_T("CopyFile失败：%d[src:%s, dst:%s]\n"), GetLastError(), srcFilePath, dstFilePath);
            return false;
        }
        return true;
    };
    tstring strSelectDir = DIR_SELECT;
    tstring dstDllFile = strSelectDir + FILE_DLL;
    tstring dstCfgFile = strSelectDir + FILE_DLL_CONFIG;
    if (!fnCopyFile(FILE_DLL, dstDllFile.c_str())
        || !fnCopyFile(FILE_DLL_CONFIG, dstCfgFile.c_str())
        || !fnCopyFile(FILE_SENDER, (strSelectDir + FILE_SENDER).c_str())
        || !fnCopyFile(FILE_SENDER_CONFIG, (strSelectDir + FILE_SENDER_CONFIG).c_str())
    ) {
        PAUSE_RETURN;
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
    
    //确保配置文件中的进程已关闭
    PrintDebugString(true, _T("安装之前确保配置文件中的进程已关闭"));
    tifstream ifs(dstCfgFile);
    if (ifs.fail()) {
        PrintDebugString(false, _T("ifs.fail():%s"), ErrWrap{}().c_str());
        PAUSE_RETURN;
    }
    tstringstream ss;
    tstring strHookProc;
    while (std::getline(ifs, strHookProc))
    {
        bool open = true;
        isProcessOpen(strHookProc.c_str(), open);
        if (open) {
            ss << _T("请关闭：") << strHookProc << std::endl;
        }
        strHookProc.clear();
    }
    ifs.close();
    PrintDebugString(true, ss.str().c_str());
    if (!ss.str().empty()) {
        MessageBox(NULL, _T("请先关闭命令行提示的软件"), _T("提示"), MB_ICONWARNING);
        PAUSE_RETURN;
    }
    
    PrintDebugString(true, _T("第三步：安装dll：%s"), dstDllFile.c_str());
    if (InstallProvider(dstDllFile.c_str())) {
        MessageBox(NULL, _T("安装分层协议及其协议链成功"), _T("提示"), MB_ICONINFORMATION);
    }
    else {
        MessageBox(NULL, _T("安装分层协议及其协议链失败"), _T("错误"), MB_ICONWARNING);
    }
    PAUSE_RETURN;
}
