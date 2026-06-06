{ lib
, stdenv
, pkg-config
, libva
, driverName ? "virtio_gpu_drv_video.so"
}:

let
  abi = import ./libva-abi.nix {
    inherit lib;
    libvaVersion = libva.version;
  };

  source = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions [
      ../src/virtio_gpu_drv_video.c
      ../tests/core-fake-driver.h
      ../tests/core-libva-probe.c
      ../tests/fake-virtio-gpu-driver.c
      ../tests/harness.c
    ];
  };
in

stdenv.mkDerivation {
  pname = "virgl-vaapi-compat-fake-driver-harness";
  version = "0-unstable";

  src = source;

  strictDeps = true;
  dontConfigure = true;

  nativeBuildInputs = [ pkg-config ];
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

    mkdir -p build/real build/test

    common_cflags="$(pkg-config --cflags libva) \
      -DVIRGL_VAAPI_COMPAT_ABI_MAJOR=${toString abi.initMajor} \
      -DVIRGL_VAAPI_COMPAT_ABI_MINOR=${toString abi.initMinor} \
      -std=c11 -Wall -Wextra -Werror -fPIC"

    $CC $(pkg-config --cflags libva) \
      -std=c11 -Wall -Wextra -Werror -fPIC \
      -c -o build/core-libva-probe.o tests/core-libva-probe.c

    $CC $common_cflags \
      -shared -Wl,-soname,${driverName} \
      -o build/real/${driverName} tests/fake-virtio-gpu-driver.c

    real_driver="$(pwd)/build/real/${driverName}"
    real_driver_define="-DVIRGL_VAAPI_COMPAT_REAL_DRIVER=\"$real_driver\""
    $CC $common_cflags "$real_driver_define" \
      -shared -Wl,-soname,${driverName} \
      -o build/test/${driverName} src/virtio_gpu_drv_video.c -ldl

    $CC $(pkg-config --cflags libva) \
      -std=c11 -Wall -Wextra -Werror \
      -o build/harness tests/harness.c -ldl

    runHook postBuild
  '';

  doCheck = true;
  checkPhase = ''
    runHook preCheck
    build/harness "$(pwd)/build/test/${driverName}" "$(pwd)/build/real/${driverName}" "${abi.initSymbol}"
    runHook postCheck
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p "$out/share/virgl-vaapi-compat"
    cat > "$out/share/virgl-vaapi-compat/fake-driver-harness.txt" <<EOF
fake-driver harness passed with libva ${libva.version} and ${abi.initSymbol}
EOF
    runHook postInstall
  '';

  meta = {
    description = "Fake-driver behavior check for virgl-vaapi-compat";
    license = lib.licenses.asl20;
    platforms = lib.platforms.linux;
  };
}
