// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "stdafx.h"
#include "FileSystem.h"

std::wstring FileSystem::GetBasePath(const std::wstring& path)
{
    std::wstring pathCopy(path);
    wchar_t *basePath = &pathCopy[0];
    if (FAILED(PathCchRemoveFileSpec(basePath, pathCopy.length() + 1)))
    {
        throw std::invalid_argument("Invalid input path.");
    }

    return std::wstring(basePath);
}


std::wstring FileSystem::GetFullPath(const std::wstring& path)
{
    wchar_t fullPath[MAX_PATH];
    if (GetFullPathName(path.c_str(), ARRAYSIZE(fullPath), fullPath, NULL) == 0)
    {
        throw std::invalid_argument("Invalid input file path.");
    }
    return std::wstring(fullPath);
}

std::wstring FileSystem::CreateSubFolder(const std::wstring& parentPath, const std::wstring& subFolderName)
{
    std::wstring errorMessageW = L"Could not create a sub-folder of " + parentPath + L".";
    std::string errorMessage(errorMessageW.begin(), errorMessageW.end());

    wchar_t subFolderPath[MAX_PATH];
    if (FAILED(PathCchCombine(subFolderPath, ARRAYSIZE(subFolderPath), parentPath.c_str(), (subFolderName + L"\\").c_str())))
    {
        throw std::runtime_error(errorMessage);
    }

    if (CreateDirectory(subFolderPath, NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        throw std::runtime_error(errorMessage);
    }

    return std::wstring(subFolderPath);
}

std::wstring FileSystem::CreateTempFolder()
{
    std::wstring errorMessageW = L"Could not get a temporary folder. Try specifying one in the command line.";
    std::string errorMessage(errorMessageW.begin(), errorMessageW.end());

    wchar_t tmpDirRaw[MAX_PATH];
    auto returnValue = GetTempPath(MAX_PATH, tmpDirRaw);
    if (returnValue > MAX_PATH || (returnValue == 0))
    {
        throw std::runtime_error(errorMessage);
    }

    // Get a random folder to drop the files
    GUID guid = { 0 };
    if (FAILED(CoCreateGuid(&guid)))
    {
        throw std::runtime_error(errorMessage);
    }

    wchar_t guidRaw[MAX_PATH];
    if (StringFromGUID2(guid, guidRaw, ARRAYSIZE(guidRaw)) == 0)
    {
        throw std::runtime_error(errorMessage);
    }

    return CreateSubFolder(tmpDirRaw, guidRaw);
}