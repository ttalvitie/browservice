# Windows build

- Build a CEF distribution with patches that make the embedded Chromium use an in-memory text-only clipboard that Browservice can access:
    - Follow the instructions in https://bitbucket.org/chromiumembedded/cef/wiki/AutomatedBuildSetup.md#markdown-header-windows-configuration for building CEF on Windows using the `automate-git.py` script from https://bitbucket.org/chromiumembedded/cef/raw/master/tools/automate/automate-git.py. Use flags `--url=https://bitbucket.org/toptalvitie/cef.git --branch=4664` to make the script use the CEF source code with Browservice patches from https://bitbucket.org/toptalvitie/cef/src/4664/. Make sure to build in a short and safe path such as `C:\code`
    - After `automate-git.py` has run successfully, copy the created binary distribution tarball named `cef_binary_XYZ+XYZ+chromium-XYZ_windowsXYZ.tar.bz2` from `chromium\src\cef\binary_distrib` to this directory.

TODO 
