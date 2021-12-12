# Windows build

- Build a CEF distribution with patches that make the embedded Chromium use an in-memory text-only clipboard that Browservice can access:
    - Follow the instructions for building CEF using the automated method in in https://bitbucket.org/chromiumembedded/cef/wiki/BranchesAndBuilding.md#markdown-header-automated-method , but add flags `--url=https://bitbucket.org/toptalvitie/cef.git --branch=4664` to the `automate-git.py` command to use the patched CEF source code from https://bitbucket.org/toptalvitie/cef/src/4664/ . Also remember to build for the correct architecture (for x64, use `--x64-build` flag)

TODO 
