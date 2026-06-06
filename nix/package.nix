{ lib
, stdenv
, pkg-config
, libva
, binutils
, driverName ? "virtio_gpu_drv_video.so"
, realDriverPath ? "/run/opengl-driver/lib/dri/virtio_gpu_drv_video.so"
}:

let
  _realDriverPathIsAbsolute = lib.assertMsg
    (lib.hasPrefix "/" realDriverPath)
    "virgl-vaapi-compat: realDriverPath must be an absolute path, got '${realDriverPath}'";

  abi = import ./libva-abi.nix {
    inherit lib;
    libvaVersion = libva.version;
  };

  realDriverDefine =
    assert _realDriverPathIsAbsolute;
    lib.escapeShellArg "-DVIRGL_VAAPI_COMPAT_REAL_DRIVER=\"${realDriverPath}\"";

  source = lib.fileset.toSource {
    root = ../.;
    fileset = ../src/virtio_gpu_drv_video.c;
  };
in

stdenv.mkDerivation {
  pname = "virgl-vaapi-compat";
  version = "0-unstable";

  src = source;

  strictDeps = true;
  dontConfigure = true;

  nativeBuildInputs = [ pkg-config ];
  nativeCheckInputs = [ binutils ];
  buildInputs = [ libva.dev ];

  buildPhase = ''
    runHook preBuild

    pkg_config_version="$(pkg-config --modversion libva)"
    case "$pkg_config_version" in
      ${abi.expectedPkgConfigVersionPrefix}*|${libva.version})
        ;;
      *)
        echo "libva package ${libva.version} maps to ${abi.initSymbol}, but pkg-config reports $pkg_config_version" >&2
        exit 1
        ;;
    esac

    $CC $(pkg-config --cflags libva) \
      -DVIRGL_VAAPI_COMPAT_ABI_MAJOR=${toString abi.initMajor} \
      -DVIRGL_VAAPI_COMPAT_ABI_MINOR=${toString abi.initMinor} \
      ${realDriverDefine} \
      -std=c11 -Wall -Wextra -Werror -fPIC \
      -shared -Wl,-soname,${driverName} \
      -o ${driverName} src/virtio_gpu_drv_video.c -ldl

    runHook postBuild
  '';

  doCheck = true;
  checkPhase = ''
    runHook preCheck

    init_symbols="$(${binutils}/bin/nm -D --defined-only ${driverName} \
      | sed -n 's/.* \(__vaDriverInit_[0-9][0-9]*_[0-9][0-9]*\)$/\1/p')"
    if [ "$init_symbols" != "${abi.initSymbol}" ]; then
      echo "Expected exactly one libva init symbol, ${abi.initSymbol}; got:" >&2
      printf '%s\n' "$init_symbols" >&2
      exit 1
    fi

    runHook postCheck
  '';

  installPhase = ''
    runHook preInstall

    install -D -m0755 ${driverName} "$out/lib/dri/${driverName}"
    mkdir -p "$out/share/virgl-vaapi-compat"
    cat > "$out/share/virgl-vaapi-compat/libva-abi.env" <<EOF
LIBVA_VERSION=${libva.version}
LIBVA_INIT_ABI_MAJOR=${toString abi.initMajor}
LIBVA_INIT_ABI_MINOR=${toString abi.initMinor}
LIBVA_INIT_SYMBOL=${abi.initSymbol}
EOF

    runHook postInstall
  '';

  passthru = {
    inherit abi driverName realDriverPath;
    initSymbol = abi.initSymbol;
  };

  meta = {
    description = "VA-API compatibility shim for virgl/virtio-gpu video decode";
    homepage = "https://github.com/vicondoa/virgl-vaapi-compat";
    license = lib.licenses.asl20;
    platforms = lib.platforms.linux;
  };
}
