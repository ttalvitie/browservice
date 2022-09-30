import os
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

def fail(msg):
    print("FAIL: " + msg, file=sys.stderr)
    sys.exit(1)

def log(msg):
    print("--- " + msg, file=sys.stderr)

def run():
    if len(sys.argv) != 6:
        fail("Usage: python build_windows.py windows32|windows64 BRANCH|COMMIT|TAG PATCHED_CEF_TARBALL BUILD_DIR OUTPUT")

    arch = sys.argv[1]
    if arch not in ["windows32", "windows64"]:
        fail(f"Invalid arch '{arch}'")

    checkout = sys.argv[2]

    cef_tarball_path = sys.argv[3]
    if not os.path.isfile(cef_tarball_path):
        fail(f"Given patched CEF tarball '{cef_tarball_path}' is not a file")

    build_dir = sys.argv[4]
    build_dir = os.path.abspath(build_dir)
    if os.path.exists(build_dir):
        fail(f"Given build dir path '{build_dir}' already exists")
    if len(build_dir) > 20:
        fail(f"Given build dir path '{build_dir}' is longer than 20 characters, which may cause issues")

    output_path = sys.argv[5]
    if os.path.exists(output_path):
        fail(f"Output file '{output_path}' already exists")

    for command in ["git", "tar", "python", "cmake"]:
        if shutil.which(command) is None:
            fail(f"Required command '{command}' not found in path")

    log(f"Creating build dir '{build_dir}'")
    os.mkdir(build_dir)

    log(f"Locating repository root")
    repo_root = subprocess.check_output(["git", "rev-parse", "--show-toplevel"], cwd=os.path.dirname(os.path.realpath(__file__)))
    repo_root = repo_root.decode("UTF-8")
    assert repo_root.endswith("\n")
    repo_root = repo_root[:-1]
    repo_root = os.path.realpath(repo_root)

    src_tarball_path = os.path.join(build_dir, "browservice.tar.gz")
    log(f"Generating tarball of branch/commit/tag '{checkout}' from git repository '{repo_root}' to '{src_tarball_path}'")
    subprocess.check_call(["git", "archive", "--output", src_tarball_path, "--", checkout], cwd=repo_root)

    src_dir = os.path.join(build_dir, "src")
    log(f"Creating directory '{src_dir}' and extracting the source tarball '{src_tarball_path}' there")
    os.mkdir(src_dir)
    subprocess.check_call(["tar", "xf", os.path.join("..", "browservice.tar.gz")], cwd=src_dir) 

    cef_initial_extract_root = os.path.join(src_dir, "winbuild", "cef_extract")
    log(f"Creating directory '{cef_initial_extract_root}' and extracting CEF tarball '{cef_tarball_path}' there")
    os.mkdir(cef_initial_extract_root)
    subprocess.check_call(["tar", "xf", cef_tarball_path, "-C", cef_initial_extract_root])

    log(f"Identifying CEF binary distribution directory in {cef_initial_extract_root}")
    cef_initial_extract_dir = []
    for dirname in os.listdir(cef_initial_extract_root):
        if os.path.isdir(os.path.join(cef_initial_extract_root, dirname)):
            cef_initial_extract_dir.append(dirname)
    if len(cef_initial_extract_dir) != 1:
        fail(f"Could not identify CEF binary distribution directory in '{cef_initial_extract_root}', candidates: {cef_initial_extract_dir}")
    cef_initial_extract_dir = os.path.join(cef_initial_extract_root, cef_initial_extract_dir[0])

    cef_binary_distrib_dir = os.path.join(src_dir, "winbuild", "cef")
    log(f"Moving CEF binary distribution directory from '{cef_initial_extract_dir}' to '{cef_binary_distrib_dir}'")
    os.rename(cef_initial_extract_dir, cef_binary_distrib_dir)
    shutil.rmtree(cef_initial_extract_root)

    vcpkg_dir = os.path.join(build_dir, "vcpkg")
    log(f"Cloning vcpkg to '{vcpkg_dir}'")
    subprocess.check_call(["git", "clone", "https://github.com/microsoft/vcpkg"], cwd=build_dir)

    log(f"Bootstrapping vcpkg")
    subprocess.check_call([os.path.join(vcpkg_dir, "bootstrap-vcpkg.bat")], cwd=vcpkg_dir)

    vcpkg_arch = {"windows32": "x86-windows", "windows64": "x64-windows"}[arch]
    for packages in [["openssl"], ["pango", "poco[netssl]", "libjpeg-turbo"]]:
        packages = [package + ":" + vcpkg_arch for package in packages]
        log(f"Installing vcpkg packages {packages}")
        subprocess.check_call([os.path.join(vcpkg_dir, "vcpkg.exe"), "install"] + packages, cwd=vcpkg_dir)

    generated_code_dir = os.path.join(src_dir, "viceplugins", "retrojsvice", "gen")
    generated_html_cpp_path = os.path.join(generated_code_dir, "html.cpp")
    log(f"Creating directory '{generated_code_dir} and generating '{generated_html_cpp_path}'")
    os.mkdir(generated_code_dir)
    with open(generated_html_cpp_path, "w") as fp:
        subprocess.check_call(["python", "gen_html_cpp.py"], cwd=os.path.join(src_dir, "viceplugins", "retrojsvice"), stdout=fp)

    cef_cmake_lists_path = os.path.join(cef_binary_distrib_dir, "CMakeLists.txt")
    log(f"Adding Browservice to '{cef_cmake_lists_path}'")
    with open(cef_cmake_lists_path, "a") as fp:
        fp.write("\nadd_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/browservice)\n")

    log(f"Adding compiler path in a machine readable format to the output of '{cef_cmake_lists_path}'")
    compiler_path_log_prefix = "[GnGzAgJ5wEUZUzq7]CMAKE_CXX_COMPILER="
    compiler_path_log_suffix = "[kCVr24VJWqmdld5c]"
    with open(cef_cmake_lists_path, "a") as fp:
        fp.write(f"\nmessage(STATUS \"{compiler_path_log_prefix}${{CMAKE_CXX_COMPILER}}{compiler_path_log_suffix}\")\n")

    cef_build_dir = os.path.join(cef_binary_distrib_dir, "build")
    cmake_arch = {"windows32": "Win32", "windows64": "x64"}[arch]
    log(f"Creating CEF build directory '{cef_build_dir}' and generating build files with CMake")
    os.mkdir(cef_build_dir)
    cmake_output = subprocess.check_output(["cmake", "-G", "Visual Studio 16", "-A", cmake_arch, ".."], cwd=cef_build_dir).decode("UTF-8")
    print(cmake_output, end="")

    log(f"Detecting C++ compiler path from CMake output")
    cxx_compiler_path = cmake_output[cmake_output.index(compiler_path_log_prefix) + len(compiler_path_log_prefix):cmake_output.index(compiler_path_log_suffix)]
    cxx_compiler_path = os.path.normpath(cxx_compiler_path)
    assert os.path.isfile(cxx_compiler_path)

    log(f"Deducing MSVC redistributable DLL locations from the detected C++ compiler path '{cxx_compiler_path}'")
    i = cxx_compiler_path.lower().rindex("\\vc\\tools\\msvc\\")
    j = cxx_compiler_path.lower().index("\\bin\\", i)
    redist_arch = {"windows32": "x86", "windows64": "x64"}[arch]
    redist_root = cxx_compiler_path[:i] + "\\VC\\Redist\\MSVC\\" + cxx_compiler_path[i + 15:j] + "\\" + redist_arch
    redist_dir = []
    for dirname in os.listdir(redist_root):
        if dirname.startswith("Microsoft.VC") and dirname.endswith(".CRT"):
            redist_dir.append(dirname)
    if len(redist_dir) != 1:
        fail(f"Could not identify directory containing MSVC redistributable DLLs in '{redist_root}', candidates: {redist_dir}")
    redist_dir = os.path.join(redist_root, redist_dir[0])

    log(f"Identified MSVC redistributable DLL directory '{redist_dir}', locating DLLs")
    redists = []
    for name in (["msvcp140.dll", "vcruntime140.dll"] + (["vcruntime140_1.dll"] if arch == "windows64" else [])):
        path = os.path.join(redist_dir, name)
        assert os.path.isfile(path)
        redists.append((path, name))

    directory_props_path = os.path.join(cef_build_dir, "Directory.Build.props")
    directory_targets_path = os.path.join(cef_build_dir, "Directory.Build.targets")
    vcpkg_props_path = os.path.join(vcpkg_dir, "scripts", "buildsystems", "msbuild", "vcpkg.props")
    vcpkg_targets_path = os.path.join(vcpkg_dir, "scripts", "buildsystems", "msbuild", "vcpkg.targets")
    for (path, ref_path) in [(directory_props_path, vcpkg_props_path), (directory_targets_path, vcpkg_targets_path)]:
        log(f"Creating file '{path}' that imports '{ref_path}' to the project")
        with open(path, "wb") as fp:
            tree = ET.Element("Project")
            ET.SubElement(tree, "Import", attrib={"Project": ref_path})
            ET.ElementTree(tree).write(fp, "UTF-8")

    log(f"Building Browservice")
    subprocess.check_call(["cmake", "--build", ".", "--config", "Release", "--target", "browservice"], cwd=cef_build_dir)

    distribution_root = os.path.join(cef_build_dir, "browservice")
    initial_distribution_dir = os.path.join(distribution_root, "Release")
    distribution_name = "browservice-" + checkout + "-" + arch
    distribution_dir = os.path.join(distribution_root, distribution_name)
    log(f"Moving created Browservice distribution from '{initial_distribution_dir}' to '{distribution_dir}'")
    os.rename(initial_distribution_dir, distribution_dir)

    log(f"Copying MSVC redistributable DLLs to Browservice distribution directory '{distribution_dir}'")
    for (path, name) in redists:
        shutil.copyfile(path, os.path.join(distribution_dir, name))

    distribution_zip_basepath = os.path.join(build_dir, "browservice_distribution")
    log(f"Creating Browservice distribution ZIP archive")
    distribution_zip_path = shutil.make_archive(distribution_zip_basepath, "zip", distribution_root, distribution_name)

    log(f"Copying created ZIP archive '{distribution_zip_path}' to '{output_path}'")
    shutil.copyfile(distribution_zip_path, output_path)

    log(f"Browservice built successfully, output saved in '{output_path}'")
    log(f"NOTE: The build directory '{build_dir}' was not removed!")
    log(f"Success")

    return 0

if __name__ == "__main__":
    sys.exit(run())
