
#include <Windows.h>
#include <tchar.h>
#include <accctrl.h>
#include <AclAPI.h>
#include <string>
#include "SecurityUtils.h"
#include "dirops.h"

#define LogMessage

UINT testExecution(std::wstring strBOINCDataDirectory)
{
    DWORD               dwRes = 0;
    PACL                pACL = NULL;
    ULONGLONG           rgSidSY[(SECURITY_MAX_SID_SIZE + sizeof(ULONGLONG) - 1) / sizeof(ULONGLONG)] = {0};
    ULONGLONG           rgSidBA[(SECURITY_MAX_SID_SIZE + sizeof(ULONGLONG) - 1) / sizeof(ULONGLONG)] = {0};
    ULONGLONG           rgSidEveryone[(SECURITY_MAX_SID_SIZE + sizeof(ULONGLONG) - 1) / sizeof(ULONGLONG)] = {0};
    DWORD               dwSidSize;
    EXPLICIT_ACCESS     ea[3];
    ULONG               ulEntries = 0;
    UINT                uiReturnValue = -1;
    /*uiReturnValue = GetProperty( _T("DATADIR"), strBOINCDataDirectory );
    if (uiReturnValue) return uiReturnValue;*/
    // Initialize an EXPLICIT_ACCESS structure for all ACEs.
    ZeroMemory(&ea, 3 * sizeof(EXPLICIT_ACCESS));
    // Create a SID for the SYSTEM.
    dwSidSize = sizeof(rgSidSY);

    if (!CreateWellKnownSid(WinLocalSystemSid, NULL, rgSidSY, &dwSidSize)) {
        /*LogMessage(
            INSTALLMESSAGE_ERROR,
            NULL,
            NULL,
            NULL,
            GetLastError(),
            _T("CreateWellKnownSid Error for SYSTEM")
            );*/
        return ERROR_INSTALL_FAILURE;
    }

    // Create a SID for the Administrators group.
    dwSidSize = sizeof(rgSidBA);

    if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, rgSidBA, &dwSidSize)) {
        /* LogMessage(
             INSTALLMESSAGE_ERROR,
             NULL,
             NULL,
             NULL,
             GetLastError(),
             _T("CreateWellKnownSid Error for BUILTIN\\Administrators")
             );*/
        return ERROR_INSTALL_FAILURE;
    }

    dwSidSize = sizeof(rgSidEveryone);

    if (!CreateWellKnownSid(WinWorldSid, NULL, rgSidEveryone, &dwSidSize)) {
        return ERROR_INSTALL_FAILURE;
    }

    ulEntries = 3;
    // SYSTEM
    ea[0].grfAccessPermissions = GENERIC_ALL;
    ea[0].grfAccessMode = SET_ACCESS;
    ea[0].grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea[0].Trustee.ptstrName = (LPTSTR)rgSidSY;
    // Administrators
    ea[1].grfAccessPermissions = GENERIC_ALL;
    ea[1].grfAccessMode = SET_ACCESS;
    ea[1].grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[1].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea[1].Trustee.ptstrName = (LPTSTR)rgSidBA;
    // Everyone
    ea[2].grfAccessPermissions = GENERIC_READ;
    ea[2].grfAccessMode = SET_ACCESS;
    ea[2].grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea[2].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[2].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea[2].Trustee.ptstrName = (LPTSTR)rgSidEveryone;
    // Create a new ACL that contains the new ACEs.
    dwRes = SetEntriesInAcl(ulEntries, &ea[0], NULL, &pACL);

    if (ERROR_SUCCESS != dwRes) {
        /*LogMessage(
            INSTALLMESSAGE_INFO,
            NULL,
            NULL,
            NULL,
            GetLastError(),
            _T("SetEntriesInAcl Error")
            );
        LogMessage(
            INSTALLMESSAGE_ERROR,
            NULL,
            NULL,
            NULL,
            GetLastError(),
            _T("SetEntriesInAcl Error")
            );*/
        return ERROR_INSTALL_FAILURE;
    }

    // Set the ACL on the Data Directory itself.
    dwRes = SetNamedSecurityInfo(
                (LPWSTR)strBOINCDataDirectory.c_str(),
                SE_FILE_OBJECT,
                DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                NULL,
                NULL,
                pACL,
                NULL
            );

    if (ERROR_SUCCESS != dwRes) {
        /*LogMessage(
            INSTALLMESSAGE_INFO,
            NULL,
            NULL,
            NULL,
            GetLastError(),
            _T("SetNamedSecurityInfo Error")
            );
        LogMessage(
            INSTALLMESSAGE_ERROR,
            NULL,
            NULL,
            NULL,
            GetLastError(),
            _T("SetNamedSecurityInfo Error")
            );*/
        return ERROR_INSTALL_FAILURE;
    }

    // Set ACLs on all files and sub folders.
    RecursiveSetPermissions(strBOINCDataDirectory, pACL);

    if (pACL)
        LocalFree(pACL);

    return ERROR_SUCCESS;
}


BOOL EnableAdministratorPrivilege(LPCTSTR path)
{
    BOOL ret = FALSE;
    DWORD               dwRes = 0;
    PACL pNewDacl = NULL, pOldDacl = NULL;
    EXPLICIT_ACCESS     ea[1];
    ULONG               ulEntries = 0;
    DWORD               dwSidSize;
    ULONGLONG           rgSidBA[(SECURITY_MAX_SID_SIZE + sizeof(ULONGLONG) - 1) / sizeof(ULONGLONG)] = {0};

    do {
        if (ERROR_SUCCESS != GetNamedSecurityInfo(path, (SE_OBJECT_TYPE) SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &pOldDacl, NULL, NULL)) {
            break;
        }

        dwSidSize = sizeof(rgSidBA);

        if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, rgSidBA, &dwSidSize)) {
            break;
        }

        ulEntries = 1;
        ea[0].grfAccessPermissions = GENERIC_ALL;
        ea[0].grfAccessMode = SET_ACCESS;
        ea[0].grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea[0].Trustee.ptstrName = (LPTSTR)rgSidBA;

        // 创建新的 ACL 对象 (合并已有的 ACL 对象和刚生成的用户帐户访问控制信息)
        if (ERROR_SUCCESS != ::SetEntriesInAcl(ulEntries, &ea[0], pOldDacl, &pNewDacl)) {
            break;
        }

        dwRes = SetNamedSecurityInfo(
                    (LPWSTR)path,
                    SE_FILE_OBJECT,
                    DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                    NULL,
                    NULL,
                    pNewDacl,
                    NULL
                );

        if (dwRes != ERROR_SUCCESS) {
            break;
        }
    } while (0);

    // Set ACLs on all files and sub folders.
    RecursiveSetPermissions(tstring(path), pNewDacl);

    if (pNewDacl != NULL)
        LocalFree(pNewDacl);

    return TRUE;
}



BOOL EnableAccountPrivilege(LPCTSTR pszPath, LPCTSTR pszAccount , DWORD AccessPermissions /* = GENERIC_READ | GENERIC_EXECUTE  */, ACCESS_MODE AccessMode /* = DENY_ACCESS  */, SE_OBJECT_TYPE dwType)
{
    BOOL bSuccess = TRUE;
    PACL pNewDacl = NULL, pOldDacl = NULL;
    EXPLICIT_ACCESS ea;

    do {
        // 获取文件 (夹) 安全对象的 DACL 列表
        if (ERROR_SUCCESS != GetNamedSecurityInfo(pszPath, (SE_OBJECT_TYPE) dwType, DACL_SECURITY_INFORMATION, NULL, NULL, &pOldDacl, NULL, NULL)) {
            bSuccess  =  FALSE;
            break;
        }

        // 此处不可直接用 AddAccessAllowedAce 函数, 因为已有的 DACL 长度是固定, 必须重新创建一个 DACL 对象
        // 生成指定用户帐户的访问控制信息 (这里指定拒绝全部的访问权限)
        switch (dwType) {
        case SE_REGISTRY_KEY:
            ::BuildExplicitAccessWithName(&ea, (LPTSTR)pszAccount, AccessPermissions, AccessMode, SUB_CONTAINERS_AND_OBJECTS_INHERIT);
            break;

        case SE_FILE_OBJECT:
            ::BuildExplicitAccessWithName(&ea, (LPTSTR)pszAccount, AccessPermissions, AccessMode, SUB_CONTAINERS_AND_OBJECTS_INHERIT);
            break;

        default:
            return FALSE;
        }

        // 创建新的 ACL 对象 (合并已有的 ACL 对象和刚生成的用户帐户访问控制信息)
        if (ERROR_SUCCESS != ::SetEntriesInAcl(1, &ea, pOldDacl, &pNewDacl)) {
            bSuccess   =  FALSE;
            break;
        }

        // 设置文件 (夹) 安全对象的 DACL 列表
        if (ERROR_SUCCESS != ::SetNamedSecurityInfo((LPTSTR)pszPath, (SE_OBJECT_TYPE) dwType, DACL_SECURITY_INFORMATION, NULL, NULL, pNewDacl, NULL)) {
            bSuccess   =  FALSE;
        }
    } while (FALSE);

    // 释放资源
    if (pNewDacl != NULL) LocalFree(pNewDacl);

    return bSuccess;
}

int _tmain(void)
{
    //EnableAdministratorPrivilege(L"C:\\Windows\\System32\\DriverStore\\FileRepository");
    PSID Sid;
    GetCurrentUserSID(&Sid);
    wprintf((LPCWSTR)SecurityUtils::GetSidRawString(Sid));

    if (Sid) {
        free(Sid);
    }


    if (IsUserAdmin()) {
        wprintf(L"\n is admin");
    }

    system("pause");

    return 0;
}