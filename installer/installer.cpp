// installer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
//#include <afxwin.h>
#include <fstream>
#include <sstream>
#include <vector>

#include <Ws2spi.h>//这个头文件有点刺头，很多东西不能放在它前面
#include <SpOrder.h>
#include <TlHelp32.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ws2_32.lib")
#include "utils.h"


#define PAUSE_RETURN system("pause");return 0
#define REGDIR_ENV L"Environment"
#define KEY_ENTRYID L"LSP-CatalogEntryId"
#define FILE_DLL L"lsp.dll"
#define FILE_DLL_CONFIG L"lsp.config"
#define FILE_SENDER L"sender.exe"
#define FILE_SENDER_CONFIG L"sender.config"
#define DIR_SELECT L"C:\\Program Files\\lsp@oneclick\\"
#define EVENT_UNLOADLL L"Global\\EVENT_UNLOAD_LSPDLL"


RegWrap g_Env;
std::vector<PROCESSENTRY32W> g_hookPe32;


LPWSAPROTOCOL_INFOW GetProvider(LPINT lpTotalProtocols)
{
    DWORD dwSize = 0;
    int nError;
    LPWSAPROTOCOL_INFOW pProtoInfo = NULL;

    // 取得需要的长度
    if (WSCEnumProtocols(NULL, pProtoInfo, &dwSize, &nError) == SOCKET_ERROR)
    {
        if (nError != WSAENOBUFS) {
            DbgPrint(false, "WSCEnumProtocols:%s", ErrorString{ nError });
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

bool InstallProvider(const wchar_t* wszDllPath)   //参数：LSP的DLL的地址
{
    wchar_t wszLSPName[] = L"MyLSP";
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
    lstrcpyW(LayeredProtocolInfo.szProtocol, wszLSPName);
    //表示分层协议
    LayeredProtocolInfo.ProtocolChain.ChainLen = LAYERED_PROTOCOL;//0
    //表示方式为由提供者自己设置
    LayeredProtocolInfo.dwProviderFlags = PFL_HIDDEN;
    //安装分层协议
    GUID Layered_guid;
    CoCreateGuid(&Layered_guid);
    if (SOCKET_ERROR == WSCInstallProvider(&Layered_guid, wszDllPath, &LayeredProtocolInfo, 1, &nError))
    {
        DbgPrint(false, "安装分层协议失败：%s", ErrorString{ nError });
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
            
            DbgPrint(true, "分层协议的目录入口ID：%d", dwLayeredCatalogId);
            g_Env.set(KEY_ENTRYID, REG_DWORD, &dwLayeredCatalogId, sizeof(dwLayeredCatalogId));
            //取环境变量测试
            DWORD dwId;
            g_Env.get(KEY_ENTRYID, &dwId, sizeof(dwId));
            if (dwId == dwLayeredCatalogId) {
                DbgPrint(true, "写入环境变量成功 [key:%s, value:%d]", KEY_ENTRYID, dwId);
            }
            break;
        }
    }


    //安装协议链
    wchar_t wszChainName[WSAPROTOCOL_LEN + 1];//新分层协议的名称 over 取出来的入口模板的名称
    for (int i = 0; i < nArrayCount; i++)
    {
        swprintf_s(wszChainName, _T("%s over %s"), wszLSPName, OriginalProtocolInfo[i].szProtocol);
        ZeroMemory(OriginalProtocolInfo[i].szProtocol, sizeof(OriginalProtocolInfo[i].szProtocol));
        lstrcpyW(OriginalProtocolInfo[i].szProtocol, wszChainName);  //将这个模板的名称改成新名称↑
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
        DbgPrint(false, "安装协议链失败：%s", ErrorString{ nError });
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
        DbgPrint(false, "WSCWriteProviderOrder:%s", ErrorString{ retWSCWriteProviderOrder });
        FreeProvider(pProtoInfo);
        return false;
    }

    FreeProvider(pProtoInfo);

    return true;
}

bool RemoveProvider(DWORD dwLayeredCatalogId)
{
    DbgPrint(true, "分层协议入口ID：%d", dwLayeredCatalogId);
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
        DbgPrint(false, "没有找到相应的分层协议");
        return false;
    }
    
    int nError = 0;
    for (i = 0; i < nProtocols; i++)
    {   //查找协议链(这个协议链的[0]为分层协议提供者)
        if (pProtoInfo[i].ProtocolChain.ChainLen > 1 && pProtoInfo[i].ProtocolChain.ChainEntries[0] == dwLayeredCatalogId)
        {   //先卸载协议链
            if (SOCKET_ERROR == WSCDeinstallProvider(&pProtoInfo[i].ProviderId, &nError))
            {
                DbgPrint(false, "卸载协议链失败：%s", ErrorString{ nError });
                return false;
            }
            break;
        }
    }
    if (SOCKET_ERROR == WSCDeinstallProvider(&Layered_guid, &nError))
    {
        DbgPrint(false, "卸载分层协议失败：%s", ErrorString{ nError });
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

void CheckProcessOpen(const wchar_t* szProc, std::vector<PROCESSENTRY32W>& vec_pe32)
{
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        DbgPrint(false, "CreateToolhelp32Snapshot: %s", ErrorString{});
        return;
    }

    BOOL bMore = Process32FirstW(hProcessSnap, &pe32);
    while (bMore) 
    {
        PathRemoveExtensionW(pe32.szExeFile);
        if (lstrcmpW(pe32.szExeFile, szProc) == 0) {
            vec_pe32.emplace_back(pe32);
            break;
        }
        bMore = Process32NextW(hProcessSnap, &pe32);
    }
    CloseHandle(hProcessSnap);
}

//void __TerminateProcess(std::wstring_view strFullPath)
//{
//    PROCESSENTRY32W pe32;
//    pe32.dwSize = sizeof(PROCESSENTRY32W);
//    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
//    if (hProcessSnap == INVALID_HANDLE_VALUE) {
//        DbgPrint(false, "TH32CS_SNAPPROCESS: %s", ErrorString{});
//        return;
//    }
//    BOOL bMore = Process32FirstW(hProcessSnap, &pe32);
//    while (bMore)
//    {
//        if (lstrcmpW(pe32.szExeFile, strFullPath.substr(strFullPath.find_last_of(_T('\\')) + 1).data()) == 0) {
//            MODULEENTRY32W me32;
//            me32.dwSize = sizeof(MODULEENTRY32W);
//            HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pe32.th32ProcessID);
//            if (hModuleSnap == INVALID_HANDLE_VALUE) {
//                DbgPrint(false, "TH32CS_SNAPMODULE: %s", ErrorString{});
//                return;
//            }
//            Module32FirstW(hModuleSnap, &me32);
//            //PrintDebugString(true, _T("process name: %s"), me32.szExePath);
//            if (lstrcmpW(me32.szExePath, strFullPath.data()) == 0) {
//                DbgPrint(true, "__TerminateProcess:%s", strFullPath.data());
//                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, me32.th32ProcessID);
//                TerminateProcess(hProcess, 0);
//                WaitForSingleObject(hProcess, 1000);
//                CloseHandle(hProcess);
//                break;
//            }
//
//            CloseHandle(hModuleSnap);
//            break;
//        }
//        bMore = Process32NextW(hProcessSnap, &pe32);
//    }
//    CloseHandle(hProcessSnap);
//}

std::wstring GetUserSelectDir()
{
    std::wstring res{ L"false" };
    HRESULT hr = CoInitialize(NULL);
    DWORD dwFlags;
    IFileOpenDialog* pfd = NULL;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    hr = pfd->GetOptions(&dwFlags);
    hr = pfd->SetOptions(dwFlags | FOS_PICKFOLDERS);
    IShellItem* pShItem = NULL;
    SHCreateItemFromParsingName(L"C:\\Program Files (x86)\\OneClickClientService\\SystemProtect", NULL, IID_PPV_ARGS(&pShItem));
    pfd->SetDefaultFolder(pShItem);
    pfd->SetTitle(L"请选择存放dll的文件夹");
    hr = pfd->Show(NULL);

    if (hr == S_OK)
    {
        IShellItem* pItem = NULL;
        if (S_OK == pfd->GetResult(&pItem)) {
            LPWSTR lpSelect = NULL;
            HRESULT ret = pItem->GetDisplayName(SIGDN_FILESYSPATH, &lpSelect);
            if (ret == S_OK) {
                res = lpSelect;
                CoTaskMemFree(lpSelect);//free memory;
            }
        }
    }
    CoUninitialize();
    return res;
}

bool deleteDir(const wchar_t* dir)
{
    SHFILEOPSTRUCT FileOp{ 0 };
    FileOp.fFlags = FOF_NO_UI;
    FileOp.pFrom = dir;
    FileOp.wFunc = FO_DELETE;
    if (int op = SHFileOperationW(&FileOp) != 0) {
        DbgPrint(false, "SHFileOperation:%d", op);
        return false;
    }
    return true;
}

int wmain(int argc, wchar_t* argv[])
{
    M_ErrHand(setlocale(LOCALE_ALL, "chs") == nullptr, L"setlocale中文失败", PAUSE_RETURN);
    M_ErrHand(g_Env.open(HKEY_CURRENT_USER, REGDIR_ENV), PAUSE_RETURN);
    DWORD dwEntryId;
    bool bGetEntryId = g_Env.get(KEY_ENTRYID, &dwEntryId, sizeof(dwEntryId));

    const std::wstring strSelectDir{ DIR_SELECT };
    const std::wstring dstDllFile{ strSelectDir + FILE_DLL };
    const std::wstring dstCfgFile{ strSelectDir + FILE_DLL_CONFIG };
    const std::wstring dstSenderFile{ strSelectDir + FILE_SENDER };
    

    DbgPrint(true, "第零步：获取用户请求");
    switch (MessageBoxW(NULL, L"安装：是，卸载：否，什么也不做：取消", L"提示", MB_YESNOCANCEL | MB_DEFBUTTON3 | MB_ICONQUESTION))
    {
    case IDCANCEL:
        PAUSE_RETURN;
        break;
    case IDNO:
        if (!bGetEntryId) {
            MessageBoxW(NULL, L"已经卸载", L"提示", MB_ICONINFORMATION);
            PAUSE_RETURN;
        }
        if (RemoveProvider(dwEntryId) && g_Env.del(KEY_ENTRYID)) {
            DbgPrint(true, "设置退出lsp.dll事件:%d", SetEvent(OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_UNLOADLL)));
            //__TerminateProcess(dstSenderFile);//如果sender是CSRSS进程打开的就拒接访问了
            bool res = deleteDir(DIR_SELECT);
            while (!res) { res = deleteDir(DIR_SELECT); }
            MessageBoxW(NULL, L"卸载分层协议及其协议链成功", L"提示", MB_ICONINFORMATION);
        }
        else {
            MessageBoxW(NULL, L"卸载分层协议及其协议链失败", L"错误", MB_ICONWARNING);
        }
        PAUSE_RETURN;
        break;
    default:
        break;
    }


    DbgPrint(true, "第一步：检查环境变量[%s]判断是否已经安装", KEY_ENTRYID);
    if (bGetEntryId) {
        MessageBoxW(NULL, L"检测到已经安装，请先卸载", L"提示", MB_ICONINFORMATION);
        PAUSE_RETURN;
    }


    DbgPrint(true, "第二步：拷贝dll及其资源到安全目录：%s", DIR_SELECT);
    if (!PathIsDirectory(DIR_SELECT)) {
        if (!CreateDirectory(DIR_SELECT, NULL)) {
            DbgPrint(false, "CreateDirectory:%s", ErrorString{});
            PAUSE_RETURN;
        }
    }

    auto fnCopyFile = [](const wchar_t* file, const wchar_t* dstFilePath) {
        wchar_t srcFilePath[MAX_PATH];
        GetModuleFileName(NULL, srcFilePath, MAX_PATH);
        PathRemoveFileSpec(srcFilePath);
        PathAppend(srcFilePath, file);

        if (!CopyFile(srcFilePath, dstFilePath, FALSE)) {
            DbgPrint(false, "CopyFile失败[src:%s, dst:%s, msg:%s]", srcFilePath, dstFilePath, ErrorString{});
            return false;
        }
        return true;
    };
    
    if (!fnCopyFile(FILE_DLL, dstDllFile.c_str())
        || !fnCopyFile(FILE_DLL_CONFIG, dstCfgFile.c_str())
        || !fnCopyFile(FILE_SENDER, dstSenderFile.c_str())
        || !fnCopyFile(FILE_SENDER_CONFIG, (strSelectDir + FILE_SENDER_CONFIG).c_str())
    ) {
        PAUSE_RETURN;
    }
    
    //确保配置文件中的进程已关闭
    DbgPrint(true, "安装之前确保配置文件中的进程已关闭");
    std::wifstream ifs(dstCfgFile);
    if (ifs.fail()) {
        DbgPrint(false, "ifs.fail():%s", ErrorString{});
        PAUSE_RETURN;
    }
    std::wstringstream ss;
    std::wstring strHookProc;
    while (std::getline(ifs, strHookProc))
    {
        CheckProcessOpen(strHookProc.c_str(), g_hookPe32);
        strHookProc.clear();
    }
    ifs.close();
    for (auto& i : g_hookPe32) {
        ss.clear();
        ss.str(L"");
        ss << L"请关闭：" << i.szExeFile;
        DbgPrintVar(true, ss.str().c_str());
    }
    if (!ss.str().empty()) {
        int ans = MessageBoxW(NULL, L"必须先关闭命令行提示的通讯软件，自动关闭？", L"提示", MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
        if (ans == IDYES) {
            for (auto& i : g_hookPe32) {
                BOOL res = TerminateProcess(OpenProcess(PROCESS_TERMINATE, FALSE, i.th32ProcessID), 0);
                DbgPrint(true, "测试关闭%s:%d", i.szExeFile, res);
                if (!res) {
                    DbgPrint(false, "关闭%s失败:%s", i.szExeFile, ErrorString{});
                    PAUSE_RETURN;
                }
            }
        }
        else {
            PAUSE_RETURN;
        }
    }
    
    DbgPrint(true, "第三步：安装dll：%s", dstDllFile.c_str());
    if (InstallProvider(dstDllFile.c_str())) {
        MessageBoxW(NULL, L"安装分层协议及其协议链成功", L"提示", MB_ICONINFORMATION);
    }
    else {
        MessageBoxW(NULL, L"安装分层协议及其协议链失败", L"错误", MB_ICONWARNING);
    }
    PAUSE_RETURN;
}