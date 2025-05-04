
#include <string>
#include <Windows.h>
#include <nlohmann/json.hpp>

class SnapshotRestorer {
public:
    SnapshotRestorer(const nlohmann::json& config, DWORD bufferSize);

    // Method to restore the snapshot
    bool Restore();

private:
    std::wstring sourcePath;
    std::wstring targetPath;
	std::wstring symlinkPath;   
    DWORD bufferSize;
    ULONGLONG snapshotSize;

    // Method to check if path exists
    bool PathExists(const std::wstring& path);

    // Method to check if there is enough space in the target directory
    bool HasEnoughSpace(const std::wstring& path, ULONGLONG requiredSpace);

    // Method to get the available volume size
    bool GetVolumeSize(const std::wstring& path, ULONGLONG& volumeSize);

    // Method to get a formatted error message from the error code
    std::wstring GetErrorMessage(DWORD error);

    // Method to check if the paths exist
    bool CheckPathsExist();

    // Method to check if there is enough space in the target directory
    bool CheckTargetSpace();

    // Method to check preconditions for restoring
    bool CheckRestorePreconditions();

    // Method to copy folder contents
    bool CopyFolderContents(const std::wstring& sourceFolder, const std::wstring& targetFolder);

    // Method to copy contents from a symlinked folder
    bool CopySymlinkFolderContents(const std::wstring& symlinkPath, const std::wstring& targetDir);

    // Method to restore a chunk (for a single-threaded restore in this case)
    void RestoreChunk();

    bool MountVolume();

    // Method to handle the restore process
    void PerformRestore();
};