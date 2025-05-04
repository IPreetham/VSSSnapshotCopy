#include "snapshotrestorer.h"
#include <iostream>
#include <Shlwapi.h>
#include <fileapi.h>
#include <windows.h>

#pragma comment(lib, "Shlwapi.lib")

SnapshotRestorer::SnapshotRestorer(const nlohmann::json& config, DWORD bufferSize)
    : bufferSize(bufferSize), snapshotSize(0) {
    try {
        // Helper function to safely convert UTF-8 to wstring with backslash validation
        auto utf8ToWideBackslash = [](const std::string& str) -> std::wstring {
            if (str.empty()) return L"";

            // Convert UTF-8 to wide char
            int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
            if (size <= 0) return L"";
            std::wstring result(size - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);

            // Ensure all forward slashes are converted to backslashes
            std::replace(result.begin(), result.end(), L'/', L'\\');

            // Validate no mixed slashes
            if (str.find('/') != std::string::npos && str.find('\\') != std::string::npos) {
                throw std::runtime_error("Path contains mixed slash types");
            }

            return result;
            };

        // Validate and convert paths
        if (!config.contains("shadowPath") || !config["shadowPath"].is_string()) {
            throw std::runtime_error("Missing or invalid shadowPath in config");
        }
        sourcePath = utf8ToWideBackslash(config["shadowPath"].get<std::string>());

        if (!config.contains("targetPath") || !config["targetPath"].is_string()) {
            throw std::runtime_error("Missing or invalid targetPath in config");
        }
        targetPath = utf8ToWideBackslash(config["targetPath"].get<std::string>());

        if (!config.contains("mountPath") || !config["mountPath"].is_string()) {
            throw std::runtime_error("Missing or invalid mountPath in config");
        }
        symlinkPath = utf8ToWideBackslash(config["mountPath"].get<std::string>());

        // Additional validation for UNC paths
        if (sourcePath.find(L"\\\\?\\") != 0) {
            throw std::runtime_error("shadowPath must be a UNC path starting with \\\\?\\");
        }
    }
    catch (const std::exception& e) {
        std::wcerr << L"Error loading config: " << e.what() << std::endl;
        throw; // Re-throw to prevent invalid object creation
    }
}

// Method to check if path exists
bool SnapshotRestorer::PathExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        std::wcerr << L"Path does not exist: " << path << L" - " << GetErrorMessage(GetLastError()) << std::endl;
        return false;
    }
    return true;
}

// Method to get a formatted error message from the error code
std::wstring SnapshotRestorer::GetErrorMessage(DWORD error) {
    LPWSTR msgBuffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, (LPWSTR)&msgBuffer, 0, nullptr);
    std::wstring message = msgBuffer ? msgBuffer : L"(unknown error)";
    LocalFree(msgBuffer);
    return message;
}

// Method to check if the paths exist
bool SnapshotRestorer::CheckPathsExist() {
    return PathExists(targetPath);
}

// Method to check preconditions for restoring
bool SnapshotRestorer::CheckRestorePreconditions() {
    return MountVolume() && CheckPathsExist();
}

// Method to copy folder contents
bool SnapshotRestorer::CopyFolderContents(const std::wstring& sourceFolder, const std::wstring& targetFolder) {
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFile((sourceFolder + L"\\*").c_str(), &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to find files in: " << sourceFolder << std::endl;
        return false;
    }

    do {
        const std::wstring fileOrFolderName = findFileData.cFileName;
        if (fileOrFolderName == L"." || fileOrFolderName == L"..") {
            continue; // Skip '.' and '..'
        }

        // Skip the "System Volume Information" folder
        if (fileOrFolderName == L"System Volume Information") {
            continue;
        }

        std::wstring sourcePath = sourceFolder + L"\\" + fileOrFolderName;
        std::wstring targetPath = targetFolder + L"\\" + fileOrFolderName;

        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Create the directory in the target folder
            if (!CreateDirectory(targetPath.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                std::wcerr << L"Failed to create directory: " << targetPath << std::endl;
                FindClose(hFind);
                return false;
            }
            // Recursively copy subdirectories
            if (!CopyFolderContents(sourcePath, targetPath)) {
                FindClose(hFind);
                return false;
            }
        }
        else {
            // Copy the file
            if (!CopyFile(sourcePath.c_str(), targetPath.c_str(), FALSE)) {
                std::wcerr << L"Failed to copy file: " << sourcePath << std::endl;
                FindClose(hFind);
                return false;
            }
        }
    } while (FindNextFile(hFind, &findFileData) != 0);

    FindClose(hFind);
    return true;
}

// Method to copy contents from a symlinked folder
bool SnapshotRestorer::CopySymlinkFolderContents(const std::wstring& symlinkPath, const std::wstring& targetDir) {
    if (CopyFolderContents(symlinkPath, targetDir)) {
        std::wcout << L"Successfully copied contents from symlink: " << symlinkPath << L" to " << targetDir << std::endl;
        return true;
    }
    else {
        std::wcerr << L"Failed to copy contents to target directory: " << targetDir << std::endl;
        return false;
    }
}

// Method to restore a chunk (in this case, only single thread is used)
void SnapshotRestorer::RestoreChunk() {
    std::wstring symlinkPath = L"C:\\VSSMount";  // Path to the symlinked folder
    std::wstring targetDir = targetPath;         // Target directory to copy the contents to

    std::wcout << L"Restoring from symlink: " << symlinkPath << L" to target: " << targetDir << std::endl;

    if (!CopySymlinkFolderContents(symlinkPath, targetDir)) {
        std::wcerr << L"Failed to copy the folder contents during restore." << std::endl;
    }
}

// Method to perform the restore process

bool SnapshotRestorer::MountVolume() {
    // Ensure source path ends with a backslash
    if (sourcePath.back() != L'\\') {
        sourcePath.append(L"\\");
    }

    // Prepare target directory path

    // Clean up existing target
    DWORD attrs = GetFileAttributesW(symlinkPath.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
            // Try to remove directory (will fail if not empty)
            if (!RemoveDirectoryW(symlinkPath.c_str())) {
                DWORD error = GetLastError();
                if (error == ERROR_DIR_NOT_EMPTY) {
                    std::wcerr << L"Target directory not empty, please clear it manually" << std::endl;
                }
                std::wcerr << L"RemoveDirectory failed (" << error << "): "
                    << GetErrorMessage(error) << std::endl;
                return false;
            }
        }
        else {
            // Remove file or symlink
            if (!DeleteFileW(symlinkPath.c_str())) {
                DWORD error = GetLastError();
                std::wcerr << L"DeleteFile failed (" << error << "): "
                    << GetErrorMessage(error) << std::endl;
                return false;
            }
        }
    }

    // Create the symbolic link (mklink /D)
    DWORD flags = SYMBOLIC_LINK_FLAG_DIRECTORY;
    flags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE; // Optional: Windows 10+

    BOOL result = CreateSymbolicLinkW(
        symlinkPath.c_str(),
        sourcePath.c_str(),
        flags
    );

    if (!result) {
        DWORD error = GetLastError();
        std::wcerr << L"CreateSymbolicLink failed (" << error << "): "
            << GetErrorMessage(error) << std::endl;
        return false;
    }

    // Verify the mount (optional, but good practice)
    const int maxRetries = 5;
    const int retryDelay = 500; // ms
    bool verified = false;

    for (int i = 0; i < maxRetries && !verified; i++) {
        if (i > 0) Sleep(retryDelay);

        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW((symlinkPath + L"\\*").c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            FindClose(hFind);
            verified = true;
        }
    }

    if (!verified) {
        std::wcerr << L"Failed to verify mounted volume after " << maxRetries << " attempts" << std::endl;
        DeleteFileW(symlinkPath.c_str());
        return false;
    }

    std::wcout << L"Successfully mounted " << sourcePath << L" to " << symlinkPath << std::endl;
    return true;
}


void SnapshotRestorer::PerformRestore() {
    if (!CheckRestorePreconditions()) {
        std::wcerr << L"Restore preconditions failed." << std::endl;
        return;
    }

    std::wcout << L"Starting restore process..." << std::endl;

    // Call RestoreChunk in single-threaded manner
    RestoreChunk();

    std::wcout << L"Restore complete." << std::endl;
}

// Method to restore the snapshot (entry point)
bool SnapshotRestorer::Restore() {
    PerformRestore();
    return true;
}
