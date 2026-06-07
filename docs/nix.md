# Nix and NixOS notes

The C shim is not Nix-specific. Nix is useful because it can package both the
shim and the real driver path reproducibly, but the runtime model is the same as
on any other distribution: libva loads a shim named `virtio_gpu_drv_video.so`,
and the shim `dlopen()`s the real driver.

## Flake outputs

The generic shim integration points are:

| Output | Purpose |
| --- | --- |
| `.#virgl-vaapi-compat` / `.#default` | The generic `virtio_gpu_drv_video.so` shim package. |
| `.#compatibility-report` | JSON report of the evaluated generic graphics stack versions and derived libva init symbol. |
| `.#checks.<system>.default` | Build-time drift gate: compiles the shim, checks the exported libva ABI symbol, and runs the fake-driver behavior harness. |
| `overlays.default` | Adds the generic shim package and compatibility report to `pkgs` for downstream consumers. |

Examples:

```bash
nix build github:vicondoa/virgl-vaapi-compat#virgl-vaapi-compat
nix build github:vicondoa/virgl-vaapi-compat#compatibility-report
cat result/compatibility-report.json
```

## Packaging model

At build time, pass the real driver path with `REAL_DRIVER=...`:

```bash
make REAL_DRIVER=/nix/store/...-mesa-.../lib/dri/virtio_gpu_drv_video.so
```

At runtime, put the shim's `lib/dri` directory before the normal driver
directories:

```bash
LIBVA_DRIVER_NAME=virtio_gpu \
LIBVA_DRIVERS_PATH=/nix/store/...-virgl-vaapi-compat/lib/dri${LIBVA_DRIVERS_PATH:+:$LIBVA_DRIVERS_PATH} \
app
```

The real driver path should point to Mesa's actual virtio-gpu VA driver, not to
the shim. Avoid recursive loading.

## Development loop

For local development in a Nix shell or equivalent environment:

```bash
make clean
make
make test
VIRGL_VAAPI_COMPAT_DEBUG=1 \
LIBVA_DRIVER_NAME=virtio_gpu \
LIBVA_DRIVERS_PATH=$PWD/build${LIBVA_DRIVERS_PATH:+:$LIBVA_DRIVERS_PATH} \
vainfo
```

The Makefile derives the libva backend ABI from the `VA_MAJOR_VERSION` and
`VA_MINOR_VERSION` macros in the active libva headers; `pkg-config` is used to
find those headers. For libva 2.23.x, that produces `__vaDriverInit_1_23`.
Build with the same libva major/minor ABI expected by the runtime environment.

The Nix package follows the same evidence-based policy: version changes are
allowed when the active headers compile, the expected init symbol is exported,
and the fake-driver harness passes. Actual API or behavior drift fails the
package/check instead of relying on a fixed allowlist of exact versions.

## VM integration

In a NixOS guest, this shim is usually most useful when the guest already has a
working virgl/virtio-gpu VA-API stack. The motivating configuration had
virtio-gpu video enabled so that `vainfo` exposed H.264 profiles. The shim only
changes exported descriptor metadata after VA-API is in use.

A conservative integration is to wrap only the affected application with
`LIBVA_DRIVERS_PATH` and `LIBVA_DRIVER_NAME` rather than replacing the global VA
driver search path for the whole system.

Downstream clients should wrap only their affected application with
`LIBVA_DRIVERS_PATH` and `LIBVA_DRIVER_NAME`. Application-specific packages,
browser policies, and launch wrappers belong outside this generic shim
repository.

## Updating and garbage collection

Because `REAL_DRIVER` is an absolute path, a Mesa upgrade can change the real
driver store path. Rebuild the shim whenever the target Mesa/libva closure
changes. If an old store path is garbage-collected and the shim still points to
it, initialization will fail with a `dlopen real virtio_gpu VA driver failed`
message.
