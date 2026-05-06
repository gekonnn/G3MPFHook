> [!IMPORTANT]
> This project is still WIP. Some core functionality may be missing, and it is not intended for general use yet.

# G3MPFHook
This is a dynamic link library for Gothic 3 game, with features including but not limited to on-the-fly pack file mounting without game files modification and a feature-rich ImGui panel, making use of many engine features.
It also provides mod debugging tools, such as template or quest preview.

## Usage
Inject the DLL into the game process using any DLL injector. Menu panel can be accessed under F1.<br>
To confirm the injection was successful, you should be able to see a "G3MPFHook ..." label on top of the game's main menu (visible only if injected before game window creation), and a notification should pop-up in top-left corner of the game window.

## Run prerequisites
G3MPFHook requires all patches below to be installed for proper functionality:
- Community Patch 1.75.14
- Update Pack
- Parallel Universe Patch

## Build prerequisites
- Visual Studio 2022 Community (preferably)
- CMake (for gothic3sdk solution generation)
- [DirectX Software Development Kit (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=6812)
- [gothic3sdk](https://github.com/georgeto/gothic3sdk/) by georgeto
- cppzmq
- Microsoft Detours
- Dear ImGui

## Building
This project uses Visual Studio 2022 for compilation and vcpkg for installation of cppzmq, Dear ImGui and Microsoft Detours. However, gothic3sdk's solution files must be generated manually (you can find the instructions on its github page).'

### 1. Clone the G3MPFHook repository:<br>
```bat
git clone https://github.com/gekonnn/g3mpfhook.git
cd g3mpfhook
```
### 2. Generate gothic3sdk solution files
Before cloning gothic3sdk, make sure you're cd'd into the g3mpfhook directory.

```bat
git clone https://github.com/georgeto/gothic3sdk
cd gothic3sdk
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32 -DG3SDK_BINDING_ONLY=ON
cmake --build build --config Release -j
```

### 3. Setup vcpkg (you can skip this step if you already have vcpkg installed globally)

First, cd into any directory you want to have vcpkg in (eg. C:/vclib/)
```bat
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg.exe integrate install
```

Then, open the g3mpfhook solution, go to Tools > Command line > Developer PowerShell and execute the following commands:
```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
$env:PATH = "$env:VCPKG_ROOT;$env:PATH"
```

Visual Studio now should automatically download all required packages when building.

### 4. Build the G3MPFHook solution
Please note that Gothic 3 is a 32-bit application, so G3MPFHook must also be built as 32-bit

## FAQ  
<b>Q</b>: I'm getting "Cannot open include file: d3dx9.h: No such file or directory" error when compiling!<br>
<b>A</b>: You haven't installed DirectX SDK.

<b>Q</b>: The DirectX SDK installator fails with error S1023<br>
<b>A</b>: SDK should be installed by that point, so you can probably ignore it.

<b>Q</b>: I'm getting dependent name lookup in base class template errors (eg. "'m_pObject': undeclared identifier")<br>
<b>A</b>: Ensure you have "/Zc:twoPhase-" flag enabled in G3MPFHook > Configuration Properties > C/C++ > Command Line.