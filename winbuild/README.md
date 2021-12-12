# Windows build

- Build a CEF distribution with patches that make the embedded Chromium use an in-memory text-only clipboard that Browservice can access:
    - Follow the instructions https://bitbucket.org/chromiumembedded/cef/wiki/AutomatedBuildSetup.md#markdown-header-windows-configuration for building CEF using the `automate-git.py` script from https://bitbucket.org/chromiumembedded/cef/raw/master/tools/automate/automate-git.py , using flags `--url=https://bitbucket.org/toptalvitie/cef.git --branch=4664` to make the script use the CEF source code with Browservice patches from https://bitbucket.org/toptalvitie/cef/src/4664/ .

TODO 
