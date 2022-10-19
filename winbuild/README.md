# Windows build

- Install Microsoft Visual Studio 2019
- Build a CEF distribution with patches that make the embedded Chromium use an in-memory text-only clipboard that Browservice can access (note that this takes a lot of time, memory and disk space):
    - Ensure that you have Python3 and the Visual Studio 2019 components mentioned in https://bitbucket.org/chromiumembedded/cef/wiki/AutomatedBuildSetup.md#markdown-header-windows-configuration installed
    - Run the script [tools/build_patched_cef.py](../tools/build_patched_cef.py) in a Command Prompt using a command like `python build_patched_cef.py C:\build patched_cef_windows64.tar.bz2 windows64` (replace `windows64` by `windows32` for 32-bit build; you may also replace `C:\build` with another short build directory path)
    - After the script has finished (typically after running for multiple hours), the patched CEF distribution is created in `patched_cef_windows64.tar.bz2`. You may remove the created build directory (`C:\build` in the command above); it will not be needed after the distribution has been built
- Install Browservice dependencies:
    - Install CMake
    - Install Microsoft vcpkg https://github.com/microsoft/vcpkg with Visual Studio integration
    - Install required vcpkg packages using the following commands (these commands install both 32-bit and 64-bit versions; you may remove the ones not relevant for your build):
        ```
        vcpkg install openssl:x86-windows openssl:x64-windows
        vcpkg install pango:x86-windows pango:x64-windows poco[netssl]:x86-windows poco[netssl]:x64-windows libjpeg-turbo:x86-windows libjpeg-turbo:x64-windows
        ```
- Generate files needed to build Browservice:
    - Generate HTML template files by opening a command prompt in the `viceplugins\retrojsvice` directory of the working copy and running the following commands:
        ```
        mkdir gen
        python gen_html_cpp.py > gen\html.cpp
        ```
    - Extract the previously built CEF binary distribution tarball to this directory (this tarball contains the CEF binary distribution in a single directory, named similarly to the tarball).
    - Edit the `CMakeLists.txt` file in the CEF binary distribution directory, appending the line `add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/browservice)` to the end of the file.
    - Create a subdirectory named ´build´ for the CEF binary distribution directory, navigate to it in a commmand prompt and run one of the commands below:
        ```
        # For 32-bit builds
        cmake -G "Visual Studio 16" -A Win32 ..

        # For 64-bit builds
        cmake -G "Visual Studio 16" -A x64 ..
        ```
        This creates a Visual Studio solution file `cef.sln` that we will use to build Browservice.
- Build Browservice:
    - Use Visual Studio to open the solution `cef.sln` in the `build` subdirectory of the CEF directory.
    - If you want to do a release build instead of a debug build, change the build configuration to `Release`.
    - In the Solution Explorer, right-click `browservice` project and click Build. The complete Browservice binary distribution is generated in the subdirectory `browservice\Release` (or `browservice\Debug` for debug builds) under `build`.
- If you want to create an official binary distribution:
    - Rename the `Release` directory to `browservice-v0.9.4.0-windowsXX` (replace `XX` by 32 or 64 depending on the target architecture)
    - Copy the redistributable DLL files `msvcp140.dll`, `vcruntime140.dll` and `vcruntime140_1.dll` (the last one only on 64-bit builds; it does not exist on 32-bit) to the directory from MSVC redist folder of the correct architecture (something like `C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Redist\MSVC\14.29.30133\xYY` where `YY` is `86` or `64` depending on the target architecture).
    - Compress the directory into a zip file with same name with `.zip` appended.
- There is also a script [tools/build_windows.py](../tools/build_windows.py) for building the official Browservice binary distribution for Windows automatically
