# Browservice on Nix/NixOS

This document explains how to build and run Browservice using Nix, and how to deploy it as a NixOS service.

## Quick Start

### Run directly (no installation required)

```bash
nix run github:ttalvitie/browservice
```

Then open http://127.0.0.1:8080 in your legacy browser.

### Build locally

```bash
# Clone the repository
git clone https://github.com/ttalvitie/browservice.git
cd browservice

# Build
nix build

# Run
./result/bin/browservice
```

## Installation

### Add to your flake-based NixOS configuration

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    browservice.url = "github:ttalvitie/browservice";
  };

  outputs = { self, nixpkgs, browservice, ... }: {
    nixosConfigurations.myhost = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
        ./configuration.nix
        browservice.nixosModules.default
      ];
    };
  };
}
```

### Add to Home Manager or as a package

```nix
{
  environment.systemPackages = [
    inputs.browservice.packages.${system}.default
  ];
}
```

## NixOS Module Configuration

### Minimal configuration

```nix
{
  services.browservice = {
    enable = true;
  };
}
```

This starts browservice listening on `127.0.0.1:8080`.

### Full configuration example

```nix
{
  services.browservice = {
    enable = true;

    # Open port 8080 in the firewall
    openFirewall = true;

    # Run as a specific user (default: browservice)
    user = "browservice";
    group = "browservice";

    # Persistent data directory (set to null for incognito mode)
    dataDir = "/var/lib/browservice";

    settings = {
      # Listen on all interfaces
      listenAddress = "0.0.0.0:8080";

      # Start page for new windows
      startPage = "https://example.com";

      # Maximum concurrent browser windows
      windowLimit = 16;

      # Initial zoom level
      initialZoom = 1.0;

      # Font rendering mode
      # Options: "no-antialiasing", "antialiasing", "antialiasing-subpixel-rgb",
      #          "antialiasing-subpixel-bgr", "antialiasing-subpixel-vrgb",
      #          "antialiasing-subpixel-vbgr", "system"
      browserFontRenderMode = "antialiasing";

      # Block file:// URL scheme (security)
      blockFileScheme = true;

      # Use dedicated Xvfb server
      useDedicatedXvfb = true;

      # Show control bar
      showControlBar = true;

      # Navigation buttons: "yes", "no", or "auto"
      showSoftNavigationButtons = "auto";

      # Custom user agent (null for CEF default)
      userAgent = null;

      # Domains to skip certificate checks (use with caution)
      certificateCheckExceptions = [];

      # Extra Chromium arguments
      chromiumArgs = [];
    };

    vicePlugin = {
      # Plugin filename
      name = "retrojsvice.so";

      # Image quality: 10-100 or "PNG"
      defaultQuality = "PNG";

      # HTTP basic auth (null to disable, or "user:password")
      httpAuth = null;

      # Maximum HTTP server threads
      httpMaxThreads = 100;

      # Forward back/forward buttons
      navigationForwarding = true;

      # Show quality selector widget
      qualitySelector = true;
    };

    # Additional command-line arguments
    extraArgs = [];
  };
}
```

### Configuration for public access with authentication

```nix
{
  services.browservice = {
    enable = true;
    openFirewall = true;

    settings = {
      listenAddress = "0.0.0.0:8080";
      startPage = "https://www.google.com";
    };

    vicePlugin = {
      # Require authentication
      httpAuth = "admin:secretpassword";

      # Use JPEG compression for better performance
      defaultQuality = 70;
    };
  };
}
```

### Kiosk mode configuration

```nix
{
  services.browservice = {
    enable = true;

    # No persistent storage
    dataDir = null;

    settings = {
      listenAddress = "127.0.0.1:8080";
      startPage = "https://your-kiosk-app.example.com";
      windowLimit = 1;
      showControlBar = false;
      showSoftNavigationButtons = "no";
    };
  };
}
```

## Available Options Reference

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `enable` | bool | `false` | Enable the browservice systemd service |
| `package` | package | (from flake) | The browservice package to use |
| `openFirewall` | bool | `false` | Open the HTTP listen port in the firewall |
| `user` | string | `"browservice"` | User account to run the service |
| `group` | string | `"browservice"` | Group to run the service |
| `dataDir` | path or null | `"/var/lib/browservice"` | Data directory for cookies/cache (null = incognito) |
| `settings.listenAddress` | string | `"127.0.0.1:8080"` | HTTP server listen address |
| `settings.startPage` | string | `"about:blank"` | Initial page URL for new windows |
| `settings.windowLimit` | int | `32` | Maximum concurrent browser windows |
| `settings.initialZoom` | float | `1.0` | Default zoom factor |
| `settings.blockFileScheme` | bool | `true` | Block file:// URLs |
| `settings.useDedicatedXvfb` | bool | `true` | Use dedicated Xvfb X server |
| `settings.showControlBar` | bool | `true` | Show the control bar |
| `settings.showSoftNavigationButtons` | enum | `"auto"` | Show navigation buttons |
| `settings.browserFontRenderMode` | enum | `"antialiasing"` | Font rendering mode |
| `settings.userAgent` | string or null | `null` | Custom User-Agent header |
| `settings.certificateCheckExceptions` | list of strings | `[]` | Domains to skip cert checks |
| `settings.chromiumArgs` | list of strings | `[]` | Extra Chromium arguments |
| `vicePlugin.name` | string | `"retrojsvice.so"` | Vice plugin filename |
| `vicePlugin.defaultQuality` | int or "PNG" | `"PNG"` | Image quality (10-100 or PNG) |
| `vicePlugin.httpAuth` | string or null | `null` | HTTP basic auth credentials |
| `vicePlugin.httpMaxThreads` | int | `100` | Max HTTP server threads |
| `vicePlugin.navigationForwarding` | bool | `true` | Forward back/forward buttons |
| `vicePlugin.qualitySelector` | bool | `true` | Show quality selector widget |
| `extraArgs` | list of strings | `[]` | Additional CLI arguments |

## Development

Enter a development shell with all dependencies:

```bash
nix develop
```

Build and test:

```bash
nix build
./result/bin/browservice --help
```

Run the test suite:

```bash
nix flake check
```

## Supported Platforms

- `x86_64-linux`
- `aarch64-linux`

## Notes

### Chrome Sandbox

The Chromium sandbox requires the `chrome-sandbox` binary to have setuid root permissions. When running as a NixOS service, this is handled automatically. When running manually, you may need to either:

1. Set the permissions manually:
   ```bash
   sudo chown root:root ./result/lib/chrome-sandbox
   sudo chmod 4755 ./result/lib/chrome-sandbox
   ```

2. Or disable the sandbox (less secure):
   ```bash
   ./result/bin/browservice --chromium-args=--no-sandbox
   ```

### CEF (Chromium Embedded Framework)

This Nix package uses prebuilt patched CEF binaries from the official browservice releases. The patches enable:

- Custom in-memory clipboard (isolated from system clipboard)
- Configurable font rendering parameters

Building CEF from source is not practical in Nix due to the enormous build requirements (200GB+ disk space, 16GB+ RAM).
