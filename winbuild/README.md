# Windows build

- Build a CEF distribution with patches that make the embedded Chromium use an in-memory text-only clipboard that Browservice can access:
    - Follow the Windows 64-/32-bit instructions in https://bitbucket.org/chromiumembedded/cef/wiki/AutomatedBuildSetup.md , but add flags `--url=https://bitbucket.org/toptalvitie/cef.git --branch=4664` to the `automate-git.py` command to use the patched CEF source code from https://bitbucket.org/toptalvitie/cef/src/4664/

TODO 
