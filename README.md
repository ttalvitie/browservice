# Browservice for macOS (Apple Silicon)

Instructions for building Browservice on macOS (M Series Chips)

## Prerequisites
- **macOS** on Apple Silicon (ARM64) is required.
- **Xcode Command Line Tools**: Install via terminal:
  ```bash
  xcode-select --install
  ```
- **CMake**: Install via [Homebrew](https://brew.sh/):
  ```bash
  brew install cmake
  ```

## Build Instructions

### 1. Clone the repository
```bash
git clone https://github.com/sawyerthemiller/browservice-macos.git
cd browservice-macos
```

### 2. Download and Setup CEF
Run the setup script to download the required Chromium Embedded Framework binaries (approx. 300MB).
```bash
./setup_cef_mac.sh
```

### 3. Build the Project
Create a build directory and compile the project using CMake.
```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.logicalcpu)
```

### 4. Create the App Bundle
Pack the executable and helper processes into a macOS App Bundle. Ensure you are running this from your `build` directory.
```bash
../create_bundle.sh
```
This will create `browservice.app` in your current directory.

### 5. Finishing Up
You can now move the created `browservice.app` file into your apps folder and delete the cloned folder if you'd like.

## Running Browservice
You can launch the application directly from the build folder:
```bash
open browservice.app
```
Or run the executable inside the bundle (useful for seeing logs):
```bash
./browservice.app/Contents/MacOS/browservice
```
