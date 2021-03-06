// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

/*
TODO:
    - add other OSs
    - add win7
*/

#ifdef _WIN32
#define _WIN32_WINNT 0x0602

#include <windows.h>
#include <strsafe.h>
#include <Sddl.h>
#include <Userenv.h>
#include <AccCtrl.h>
#include <Aclapi.h>

#pragma comment(lib, "Userenv.lib")
#endif

#include "run.h"

#include <vector>

namespace sw
{

#ifdef _WIN32
//List of allowed capabilities for the application
// https://docs.microsoft.com/en-us/windows/desktop/api/winnt/ne-winnt-well_known_sid_type
std::vector<WELL_KNOWN_SID_TYPE> app_capabilities
{
    //WinCapabilityPrivateNetworkClientServerSid,
};

BOOL SetSecurityCapabilities(PSID container_sid, SECURITY_CAPABILITIES *capabilities, PDWORD num_capabilities);
BOOL GrantNamedObjectAccess(PSID appcontainer_sid, const path &object_name, SE_OBJECT_TYPE object_type, DWORD access_mask);

void run(const RunArgs &args)
{
    PSID sid = NULL;
    SECURITY_CAPABILITIES SecurityCapabilities = { 0 };
    DWORD num_capabilities = 0;
    SIZE_T attribute_size = 0;
    STARTUPINFOEX startup_info = { 0 };
    startup_info.StartupInfo.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info = { 0 };
    HANDLE file_handle = INVALID_HANDLE_VALUE;
    BOOL success = FALSE;

    auto container_name_s = to_wstring("sw.app." + args.pkg.getHash().substr(0, 32));
    auto container_name = container_name_s.c_str();

    auto pkg_s = to_wstring(args.pkg.toString());
    auto container_desc_s = pkg_s;
    if (pkg_s.size() > 512)
        pkg_s = container_name_s;
    if (container_desc_s.size() > 2048)
        container_desc_s = container_name_s;
    auto pkg_name = pkg_s.c_str();
    auto container_desc = container_desc_s.c_str();

    String err(1024, 0);

    do
    {
        auto wdir = args.pkg.getDirObjWdir();
        wdir = args.pkg.getDirSrc2() / "Lib";
        fs::create_directories(wdir);

        if (args.in_container)
        {
            auto result = CreateAppContainerProfile(container_name, pkg_name, container_desc, NULL, 0, &sid);
            if (!SUCCEEDED(result))
            {
                if (HRESULT_CODE(result) == ERROR_ALREADY_EXISTS)
                {
                    result = DeriveAppContainerSidFromAppContainerName(container_name, &sid);
                    if (!SUCCEEDED(result))
                    {
                        snprintf(err.data(), err.size(), "Failed to get existing AppContainer name, error code: %d", HRESULT_CODE(result));
                        break;
                    }
                }
                else
                {
                    snprintf(err.data(), err.size(), "Failed to create AppContainer, last error: %d\n", HRESULT_CODE(result));
                    break;
                }
            }

            if (!SetSecurityCapabilities(sid, &SecurityCapabilities, &num_capabilities))
            {
                snprintf(err.data(), err.size(), "Failed to set security capabilities, last error: %d\n", GetLastError());
                break;
            }

            // set permissions
            if (!GrantNamedObjectAccess(sid, wdir, SE_FILE_OBJECT, FILE_ALL_ACCESS & ~DELETE))
            {
                snprintf(err.data(), err.size(), "Failed to grant explicit access to %s\n", wdir.u8string().c_str());
                break;
            }

            InitializeProcThreadAttributeList(NULL, 1, NULL, &attribute_size);
            startup_info.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(attribute_size);

            if (!InitializeProcThreadAttributeList(startup_info.lpAttributeList, 1, NULL, &attribute_size))
            {
                snprintf(err.data(), err.size(), "InitializeProcThreadAttributeList() failed, last error: %d", GetLastError());
                break;
            }

            if (!UpdateProcThreadAttribute(startup_info.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,
                &SecurityCapabilities, sizeof(SecurityCapabilities), NULL, NULL))
            {
                snprintf(err.data(), err.size(), "UpdateProcThreadAttribute() failed, last error: %d", GetLastError());
                break;
            }
        }

        if (!CreateProcess(args.exe_path.wstring().c_str(), NULL, NULL, NULL, FALSE, CREATE_NEW_CONSOLE | EXTENDED_STARTUPINFO_PRESENT, NULL,
            wdir.wstring().c_str(),
            (LPSTARTUPINFO)&startup_info, &process_info))
        {
            snprintf(err.data(), err.size(), "Failed to create process %s, last error: %d\n", args.exe_path.u8string().c_str(), GetLastError());
            break;
        }

        success = TRUE;
    } while (0);

    if (startup_info.lpAttributeList)
        DeleteProcThreadAttributeList(startup_info.lpAttributeList);

    if (SecurityCapabilities.Capabilities)
        free(SecurityCapabilities.Capabilities);

    if (sid)
        FreeSid(sid);

    if (file_handle != INVALID_HANDLE_VALUE)
        CloseHandle(file_handle);

    if (!success)
        throw std::runtime_error(err.c_str()); // to strip nulls
}

BOOL SetSecurityCapabilities(PSID container_sid, SECURITY_CAPABILITIES *capabilities, PDWORD num_capabilities)
{
    DWORD sid_size = SECURITY_MAX_SID_SIZE;
    DWORD num_capabilities_ = app_capabilities.size() * sizeof(WELL_KNOWN_SID_TYPE) / sizeof(DWORD);
    SID_AND_ATTRIBUTES *attributes;
    BOOL success = TRUE;

    attributes = (SID_AND_ATTRIBUTES *)malloc(sizeof(SID_AND_ATTRIBUTES) * num_capabilities_);

    ZeroMemory(capabilities, sizeof(SECURITY_CAPABILITIES));
    ZeroMemory(attributes, sizeof(SID_AND_ATTRIBUTES) * num_capabilities_);

    for (unsigned int i = 0; i < num_capabilities_; i++)
    {
        attributes[i].Sid = malloc(SECURITY_MAX_SID_SIZE);
        if (!CreateWellKnownSid(app_capabilities[i], NULL, attributes[i].Sid, &sid_size))
        {
            success = FALSE;
            break;
        }
        attributes[i].Attributes = SE_GROUP_ENABLED;
    }

    if (success == FALSE)
    {
        for (unsigned int i = 0; i < num_capabilities_; i++)
        {
            if (attributes[i].Sid)
                LocalFree(attributes[i].Sid);
        }

        free(attributes);
        attributes = NULL;
        num_capabilities_ = 0;
    }

    capabilities->Capabilities = attributes;
    capabilities->CapabilityCount = num_capabilities_;
    capabilities->AppContainerSid = container_sid;
    *num_capabilities = num_capabilities_;

    return success;
}

BOOL GrantNamedObjectAccess(PSID appcontainer_sid, const path &object_name, SE_OBJECT_TYPE object_type, DWORD access_mask)
{
    EXPLICIT_ACCESS explicit_access;
    PACL original_acl = NULL, new_acl = NULL;
    DWORD status;
    BOOL success = FALSE;

    do
    {
        explicit_access.grfAccessMode = GRANT_ACCESS;
        explicit_access.grfAccessPermissions = access_mask;
        explicit_access.grfInheritance = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;

        explicit_access.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
        explicit_access.Trustee.pMultipleTrustee = NULL;
        explicit_access.Trustee.ptstrName = (TCHAR *)appcontainer_sid;
        explicit_access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        explicit_access.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;

        status = GetNamedSecurityInfo(object_name.wstring().c_str(), object_type, DACL_SECURITY_INFORMATION, NULL, NULL, &original_acl,
            NULL, NULL);
        if (status != ERROR_SUCCESS)
        {
            printf("GetNamedSecurityInfo() failed for %s, error: %d\n", object_name.u8string().c_str(), status);
            break;
        }

        status = SetEntriesInAcl(1, &explicit_access, original_acl, &new_acl);
        if (status != ERROR_SUCCESS)
        {
            printf("SetEntriesInAcl() failed for %s, error: %d\n", object_name.u8string().c_str(), status);
            break;
        }

        status = SetNamedSecurityInfo((LPTSTR)object_name.wstring().c_str(), object_type, DACL_SECURITY_INFORMATION, NULL, NULL, new_acl, NULL);
        if (status != ERROR_SUCCESS)
        {
            printf("SetNamedSecurityInfo() failed for %s, error: %d\n", object_name.u8string().c_str(), status);
            break;
        }

        success = TRUE;
    } while (0);

    // MSDN: no need to free original_acl
    //if (original_acl)
        //LocalFree(original_acl);

    if (new_acl)
        LocalFree(new_acl);

    return success;
}
#else
void run(const RunArgs &args)
{
    throw std::runtime_error("not implemented");
}
#endif

}
