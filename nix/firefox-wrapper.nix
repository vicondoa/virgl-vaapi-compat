{ lib
, stdenvNoCC
, makeWrapper
, virgl-vaapi-compat
, firefox
, aliasName ? "firefox-virgl-vaapi"
, firefoxBinary ? "firefox"
, libvaDriverName ? "virtio_gpu"
, libvaDriversPathExtra ? [ ]
, renderNode ? "/dev/dri/renderD128"
, renderer ? "virgl (virtio-gpu)"
, glVersion ? "4.6 (Compatibility Profile) Mesa"
, extraEnv ? { }
, lockedPreferences ? { }
, extraPolicies ? { }
}:

let
  firefoxName = firefox.pname or (lib.getName firefox);
  firefoxVersion = firefox.version or (lib.getVersion firefox);

  libvaDriversPath = lib.concatStringsSep ":" (
    [ "${virgl-vaapi-compat}/lib/dri" ] ++ libvaDriversPathExtra
  );

  defaultEnv = {
    LIBVA_DRIVER_NAME = libvaDriverName;
    LIBVA_DRIVERS_PATH = libvaDriversPath;
    MOZ_ENABLE_WAYLAND = "1";
  };

  wrapperEnv = defaultEnv // lib.mapAttrs (_: value: toString value) extraEnv;

  makeWrapperArgs = lib.concatStringsSep " " (
    lib.mapAttrsToList
      (name: value: "--set ${lib.escapeShellArg name} ${lib.escapeShellArg (toString value)}")
      wrapperEnv
  );

  defaultLockedPreferences = {
    "gfx.webrender.all" = true;
    "gfx.webrender.enabled" = true;
    "layers.acceleration.force-enabled" = true;
    "media.av1.enabled" = false;
    "media.ffmpeg.dmabuf-textures.enabled" = true;
    "media.ffmpeg.vaapi.enabled" = true;
    "media.ffmpeg.vaapi.force-enabled" = true;
    "media.ffmpeg.vaapi.force-surface-zero-copy" = 1;
    "media.ffvpx.enabled" = false;
    "media.hardware-video-decoding.enabled" = true;
    "media.hardware-video-decoding.force-enabled" = true;
    "media.mediasource.vp9.enabled" = false;
    "media.mediasource.webm.enabled" = false;
    "media.rdd-ffmpeg.enabled" = true;
    "media.rdd-vpx.enabled" = true;
    "media.webm.enabled" = false;
    "widget.dmabuf.force-enabled" = true;
  };

  lockedPreferencePolicies = lib.mapAttrs
    (_: value: {
      Value = value;
      Status = "locked";
    })
    (defaultLockedPreferences // lockedPreferences);

  policyDocument = {
    policies = lib.recursiveUpdate {
      DisableAppUpdate = true;
      Preferences = lockedPreferencePolicies;
    } extraPolicies;
  };

  policiesJson = builtins.toJSON policyDocument;
in

stdenvNoCC.mkDerivation {
  pname = "${firefoxName}-virgl-vaapi";
  version = firefoxVersion;

  nativeBuildInputs = [ makeWrapper ];
  dontUnpack = true;

  installPhase = ''
    runHook preInstall

    mkdir -p "$out"
    cp -a --no-preserve=ownership ${firefox}/. "$out/"
    chmod -R u+w "$out"

    main="$out/bin/${firefoxBinary}"
    if [ ! -e "$main" ]; then
      main="$(find "$out/bin" -maxdepth 1 -type f -perm -0100 | sort | head -n 1 || true)"
    fi
    if [ -z "$main" ] || [ ! -e "$main" ]; then
      echo "Could not find Firefox executable '${firefoxBinary}' in ${firefox}" >&2
      exit 1
    fi

    main_name="$(basename "$main")"
    mv "$main" "$main.virgl-vaapi-original"
    makeWrapper "$main.virgl-vaapi-original" "$main" ${makeWrapperArgs}

    if [ "$main_name" != "${aliasName}" ]; then
      rm -f "$out/bin/${aliasName}"
      ln -s "$main_name" "$out/bin/${aliasName}"
    fi

    found_app_dir=0
    for app_dir in "$out"/lib/firefox "$out"/lib/firefox-*; do
      [ -e "$app_dir" ] || continue

      if [ -L "$app_dir" ]; then
        app_target="$(readlink -f "$app_dir")"
        rm "$app_dir"
        cp -a --no-preserve=ownership "$app_target" "$app_dir"
        chmod -R u+w "$app_dir"
      fi

      [ -d "$app_dir" ] || continue
      found_app_dir=1

      mkdir -p "$app_dir/distribution"
      cat > "$app_dir/distribution/policies.json" <<'EOF'
${policiesJson}
EOF

      rm -f "$app_dir/glxtest"
      cat > "$app_dir/glxtest" <<'SH'
#!/bin/sh
out=/dev/stdout
while [ "$#" -gt 0 ]; do
  case "$1" in
    -f)
      out="/proc/self/fd/$2"
      shift 2
      ;;
    *)
      shift
      ;;
  esac
done

cat > "$out" <<'EOF'
DRM_RENDERDEVICE
${renderNode}
DRI_DRIVER
virtio_gpu
VENDOR
Mesa
RENDERER
${renderer}
VERSION
${glVersion}
TFP
TRUE
MESA_ACCELERATED
TRUE
DMABUF_MODIFIERS_XRGB
0
DMABUF_MODIFIERS_ARGB
0
DMABUF_MODIFIERS_NV12
0
DMABUF_MODIFIERS_P010
0
DMABUF_MODIFIERS_YUV420
0
TEST_TYPE
EGL
EOF
SH
      chmod 0755 "$app_dir/glxtest"
    done

    if [ "$found_app_dir" -eq 0 ]; then
      echo "Could not find a Firefox application directory under $out/lib" >&2
      exit 1
    fi

    runHook postInstall
  '';

  passthru = {
    inherit firefox glVersion policyDocument renderNode renderer virgl-vaapi-compat wrapperEnv;
  };

  meta = (firefox.meta or { }) // {
    description = "Firefox wrapped to use virgl-vaapi-compat through scoped LIBVA settings";
    mainProgram = aliasName;
  };
}
