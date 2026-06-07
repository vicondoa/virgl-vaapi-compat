# Troubleshooting

## libva does not load the shim

Check that the shim directory contains `virtio_gpu_drv_video.so` and appears
before the real driver directory:

```bash
LIBVA_DRIVER_NAME=virtio_gpu \
LIBVA_DRIVERS_PATH=/path/to/shim/lib/dri${LIBVA_DRIVERS_PATH:+:$LIBVA_DRIVERS_PATH} \
LIBVA_MESSAGING_LEVEL=2 \
vainfo
```

Also confirm the shim exports the libva init symbol expected by the runtime ABI.
A libva 2.23 runtime expects `__vaDriverInit_1_23`.

## The shim fails to initialize the real driver

Typical message:

```text
virgl-vaapi-compat: dlopen real virtio_gpu VA driver failed: ...
```

Fixes:

- rebuild with `make REAL_DRIVER=/absolute/path/to/real/virtio_gpu_drv_video.so`
- make sure that path is not the shim itself
- on Nix/NixOS, rebuild after Mesa upgrades so the compiled-in store path is
  still valid

## The hook installs but nothing is rewritten

With `VIRGL_VAAPI_COMPAT_DEBUG=1`, you should see export messages during actual
VA-API playback or export tests. If not:

- the client may not be using VA-API
- the client may not be using the virtio-gpu VA driver
- the client may not call `vaExportSurfaceHandle()` for the tested workload
- the export may not be `DRM_PRIME_2`
- the exported format may not be I420

Use `vainfo`, client logs, and [`debugging.md`](debugging.md) to confirm the
path.

## `vainfo` shows no H.264 profiles

The shim cannot add decoder support. Fix the underlying virtio-gpu/virgl video
configuration first. In the motivating environment, enabling virgl video support
made the guest VA-API driver advertise H.264 decode profiles.

## The client still falls back to software

Confirm all of the following:

- the client selected VA-API decode according to its own logs or telemetry
- the baseline failure is a DMABUF/image allocation failure at surface export
- shim debug logs show an I420 `DRM_PRIME_2` descriptor rewrite
- the issue is not a separate sandbox, display, codec, or GPU-process problem

If the shim never sees a matching export, it is not addressing the active
failure.

## Colors are wrong

This shim does not transform pixels. It only changes the descriptor's fourcc and
swaps U/V layer metadata for the known I420-to-YV12 compatibility case. If
colors are wrong, disable the shim and collect descriptor details before using it
on that stack.

## The process crashes or recurses

The most common cause is pointing `REAL_DRIVER` at the shim path, so the shim
loads itself. Keep the real driver at a distinct absolute path and install the
shim into a separate directory that is only used through `LIBVA_DRIVERS_PATH`.
