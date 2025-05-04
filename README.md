# VSS Snapshot Restorer

Restores files from Windows Volume Shadow Copies.

## What It Does
- Mounts VSS snapshots (shadow copies) so that regualar file operations can be performed
- Copies files from mounted to target location

## How to Build
1. Open `VSSSnapShotCopy.sln` in Visual Studio
2. Right-click solution and select "Build Solution" with appropriate configuration (Debug/Release) and bitness

## How to Use
update config.JSON with proper values and run the tool from command line as Administrator
Make sure that the config.json is in the same directory as the executable.

Sample json content

{ "shadowPath": "\\?\GLOBALROOT\Device\HarddiskVolumeShadowCopyX", "targetPath": "D:\RestoredFiles\", "mountPath": "C:\Temp\VSSMount" }


- **shadowPath**: Path to the shadow copy you want to restore files from. This is usually in the format `\\?\GLOBALROOT\Device\HarddiskVolumeShadowCopyX`, where `X` is the number of the shadow copy.
- **targetPath**: Path where you want to restore the files. This should be a valid directory on your system.
- **mountPath**: Path where the shadow copy will be mounted. This should be a valid directory on your system.

## In Case Any Issues Are Observed While Running the Tool
- Build the tool and run it from Visual Studio (as Administrator) using the "Local Windows Debugger" button.
- Use the `config.json` file in the `main.cpp` folder to set the parameters for the tool.
