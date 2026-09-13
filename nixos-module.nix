{ config, lib, pkgs, ... }:

let
  cfg = config.services.browservice;
  inherit (lib) mkEnableOption mkOption mkIf types optionalString concatStringsSep;

  boolToYesNo = b: if b then "yes" else "no";

  fontRenderModeType = types.enum [
    "no-antialiasing"
    "antialiasing"
    "antialiasing-subpixel-rgb"
    "antialiasing-subpixel-bgr"
    "antialiasing-subpixel-vrgb"
    "antialiasing-subpixel-vbgr"
    "system"
  ];

  navigationButtonsType = types.enum [ "yes" "no" "auto" ];

in {
  options.services.browservice = {
    enable = mkEnableOption "Browservice - browser service for legacy web browsers";

    package = mkOption {
      type = types.package;
      description = "The browservice package to use.";
    };

    openFirewall = mkOption {
      type = types.bool;
      default = false;
      description = "Whether to open the HTTP listen port in the firewall.";
    };

    user = mkOption {
      type = types.str;
      default = "browservice";
      description = "User account under which browservice runs.";
    };

    group = mkOption {
      type = types.str;
      default = "browservice";
      description = "Group under which browservice runs.";
    };

    dataDir = mkOption {
      type = types.nullOr types.path;
      default = "/var/lib/browservice";
      description = ''
        Directory for storing persistent data (cookies, cache).
        Set to null for incognito mode (no persistent storage).
      '';
    };

    settings = {
      listenAddress = mkOption {
        type = types.str;
        default = "127.0.0.1:8080";
        description = "IP address and port for the HTTP server to listen on.";
      };

      startPage = mkOption {
        type = types.str;
        default = "about:blank";
        description = "URL of the initial page for each new window.";
      };

      windowLimit = mkOption {
        type = types.ints.positive;
        default = 32;
        description = "Maximum number of browser windows that can be open at the same time.";
      };

      initialZoom = mkOption {
        type = types.float;
        default = 1.0;
        description = "Initial zoom factor for new browser windows.";
      };

      blockFileScheme = mkOption {
        type = types.bool;
        default = true;
        description = ''
          Block attempts to access local files through the file:// URI scheme.
          WARNING: There may be ways around the block; do NOT allow untrusted users to access Browservice.
        '';
      };

      useDedicatedXvfb = mkOption {
        type = types.bool;
        default = true;
        description = "Run the browser in its own Xvfb X server.";
      };

      showControlBar = mkOption {
        type = types.bool;
        default = true;
        description = "Show the control bar (disable for kiosk mode).";
      };

      showSoftNavigationButtons = mkOption {
        type = navigationButtonsType;
        default = "auto";
        description = ''
          Show navigation buttons (Back/Forward/Refresh/Home) in the control bar.
          'auto' enables unless the vice plugin implements navigation buttons.
        '';
      };

      browserFontRenderMode = mkOption {
        type = fontRenderModeType;
        default = "antialiasing";
        description = "Font render mode for the browser area.";
      };

      userAgent = mkOption {
        type = types.nullOr types.str;
        default = null;
        description = "Custom User-Agent header. Null uses CEF default.";
      };

      certificateCheckExceptions = mkOption {
        type = types.listOf types.str;
        default = [];
        description = ''
          List of domains for which SSL/TLS certificate checking is disabled.
          Only exact match is considered (e.g., 'example.com' does not cover 'www.example.com').
        '';
      };

      chromiumArgs = mkOption {
        type = types.listOf types.str;
        default = [];
        description = "Extra arguments to forward to Chromium.";
      };
    };

    vicePlugin = {
      name = mkOption {
        type = types.str;
        default = "retrojsvice.so";
        description = "Vice plugin to use for the user interface.";
      };

      defaultQuality = mkOption {
        type = types.either types.ints.positive (types.enum [ "PNG" ]);
        default = "PNG";
        description = "Initial image quality for each window (10-100 or 'PNG').";
      };

      httpAuth = mkOption {
        type = types.nullOr types.str;
        default = null;
        description = ''
          HTTP basic authentication credentials in 'USER:PASSWORD' format.
          Use 'env' to read from HTTP_AUTH_CREDENTIALS environment variable.
          Null disables authentication.
        '';
      };

      httpMaxThreads = mkOption {
        type = types.ints.positive;
        default = 100;
        description = "Maximum number of HTTP server threads.";
      };

      navigationForwarding = mkOption {
        type = types.bool;
        default = true;
        description = "Forward client back/forward buttons to the program.";
      };

      qualitySelector = mkOption {
        type = types.bool;
        default = true;
        description = "Show image quality selector widget.";
      };
    };

    extraArgs = mkOption {
      type = types.listOf types.str;
      default = [];
      description = "Extra command-line arguments to pass to browservice.";
    };
  };

  config = mkIf cfg.enable {
    users.users = mkIf (cfg.user == "browservice") {
      browservice = {
        isSystemUser = true;
        group = cfg.group;
        home = cfg.dataDir;
        createHome = cfg.dataDir != null;
        description = "Browservice daemon user";
      };
    };

    users.groups = mkIf (cfg.group == "browservice") {
      browservice = {};
    };

    networking.firewall = mkIf cfg.openFirewall {
      allowedTCPPorts = [
        (lib.toInt (lib.last (lib.splitString ":" cfg.settings.listenAddress)))
      ];
    };

    systemd.services.browservice = {
      description = "Browservice - Browser service for legacy web browsers";
      wantedBy = [ "multi-user.target" ];
      after = [ "network.target" ];

      # Required for Xvfb and xauth when using --use-dedicated-xvfb
      path = with pkgs; [
        xorg.xauth
        xorg.xorgserver  # Provides Xvfb
      ];

      serviceConfig = {
        Type = "simple";
        User = cfg.user;
        Group = cfg.group;
        ExecStart = let
          args = [
            "--use-dedicated-xvfb=${boolToYesNo cfg.settings.useDedicatedXvfb}"
            "--block-file-scheme=${boolToYesNo cfg.settings.blockFileScheme}"
            "--show-control-bar=${boolToYesNo cfg.settings.showControlBar}"
            "--show-soft-navigation-buttons=${cfg.settings.showSoftNavigationButtons}"
            "--browser-font-render-mode=${cfg.settings.browserFontRenderMode}"
            "--start-page=${cfg.settings.startPage}"
            "--window-limit=${toString cfg.settings.windowLimit}"
            "--initial-zoom=${toString cfg.settings.initialZoom}"
            "--vice-plugin=${cfg.vicePlugin.name}"
            "--vice-opt-http-listen-addr=${cfg.settings.listenAddress}"
            "--vice-opt-default-quality=${toString cfg.vicePlugin.defaultQuality}"
            "--vice-opt-http-max-threads=${toString cfg.vicePlugin.httpMaxThreads}"
            "--vice-opt-navigation-forwarding=${boolToYesNo cfg.vicePlugin.navigationForwarding}"
            "--vice-opt-quality-selector=${boolToYesNo cfg.vicePlugin.qualitySelector}"
          ]
          ++ lib.optional (cfg.dataDir != null) "--data-dir=${cfg.dataDir}"
          ++ lib.optional (cfg.settings.userAgent != null) "--user-agent=${cfg.settings.userAgent}"
          ++ lib.optional (cfg.settings.certificateCheckExceptions != [])
              "--certificate-check-exceptions=${concatStringsSep "," cfg.settings.certificateCheckExceptions}"
          ++ lib.optional (cfg.settings.chromiumArgs != [])
              "--chromium-args=${concatStringsSep "," cfg.settings.chromiumArgs}"
          ++ lib.optional (cfg.vicePlugin.httpAuth != null)
              "--vice-opt-http-auth=${cfg.vicePlugin.httpAuth}"
          ++ cfg.extraArgs;
        in "${cfg.package}/bin/browservice ${concatStringsSep " " args}";

        Restart = "on-failure";
        RestartSec = "5s";

        # Security hardening
        NoNewPrivileges = false;  # Required for chrome-sandbox setuid
        PrivateTmp = true;
        ProtectSystem = "strict";
        ProtectHome = true;
        ReadWritePaths = lib.optional (cfg.dataDir != null) cfg.dataDir;
        ProtectKernelTunables = true;
        ProtectKernelModules = true;
        ProtectControlGroups = true;
        RestrictNamespaces = false;  # Required for CEF sandbox
        LockPersonality = true;
        RestrictRealtime = true;
        SystemCallArchitectures = "native";

        # Allow binding to privileged ports and disable user namespacing for CEF
        AmbientCapabilities = [ "CAP_NET_BIND_SERVICE" ];
        CapabilityBoundingSet = [ "CAP_NET_BIND_SERVICE" ];
        PrivateUsers = false;
      };
    };

    # Create data directory and .browservice subdirectory
    systemd.tmpfiles.rules = lib.optionals (cfg.dataDir != null) [
      "d ${cfg.dataDir} 0750 ${cfg.user} ${cfg.group} -"
      "d ${cfg.dataDir}/.browservice 0750 ${cfg.user} ${cfg.group} -"
    ];
  };
}
