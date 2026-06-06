# virgl-vaapi-compat

`virgl-vaapi-compat` is a small VA-API driver compatibility shim for
virgl/virtio-gpu video decode guests. It exists for one narrow interop gap:
some stacks export decoded YUV surfaces through `vaExportSurfaceHandle()` as
`DRM_PRIME_2` three-plane `VA_FOURCC_I420`, while some VA-API clients only
accept the equivalent U/V ordering when it is described as `VA_FOURCC_YV12`.

The shim is not a decoder, a color converter, a compositor, or a replacement
for Mesa, virglrenderer, libva, or the guest graphics stack. It delegates to the
real `virtio_gpu_drv_video.so` VA driver and adjusts only the exported metadata
for the known-compatible descriptor shape.

## What it does

```text
VA-API client
    -> virgl-vaapi-compat virtio_gpu_drv_video.so
        -> real virtio_gpu_drv_video.so
            -> virgl / virtio-gpu video stack
```

At driver initialization, the shim loads the real virtio-gpu VA driver, calls
its matching `__vaDriverInit_*` symbol, and replaces only the driver's
`vaExportSurfaceHandle` vtable entry. On a successful
`VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2` export of an I420 descriptor with the
expected three-plane layout, it reports `VA_FOURCC_YV12` and swaps the U/V layer
entries. All other calls, formats, memory types, errors, and descriptors pass
through unchanged.

The original motivating client is Firefox hardware H.264 decode in a
virgl/rutabaga/crosvm guest, but the C shim itself is not Firefox-specific and
is not Nix-specific.

## Quickstart

Requirements:

- C compiler
- `make`
- `pkg-config`
- libva headers and library
- the real virtio-gpu VA driver available at a stable absolute path

Build and run the test harness:

```bash
make
make test
```

If the real driver is not at the default NixOS path
`/run/opengl-driver/lib/dri/virtio_gpu_drv_video.so`, point the build at it:

```bash
make REAL_DRIVER=/usr/lib/dri/virtio_gpu_drv_video.so
```

Install the shim into a separate driver directory and put that directory first
when launching a client:

```bash
make install PREFIX=/usr/local

LIBVA_DRIVER_NAME=virtio_gpu \
LIBVA_DRIVERS_PATH=/usr/local/lib/dri${LIBVA_DRIVERS_PATH:+:$LIBVA_DRIVERS_PATH} \
VIRGL_VAAPI_COMPAT_DEBUG=1 \
vainfo
```

Do not overwrite the only copy of the real driver with the shim. The shim must
be able to `dlopen()` the real `virtio_gpu_drv_video.so`, preferably through an
absolute path compiled with `REAL_DRIVER=...`.

## Nix and Firefox

Nix/NixOS users can package the shim by setting `REAL_DRIVER` to the real Mesa
virtio-gpu VA driver path in the target graphics closure and exposing the shim
through a higher-priority `LIBVA_DRIVERS_PATH`. The flake exposes:

```bash
nix build github:vicondoa/virgl-vaapi-compat#virgl-vaapi-compat
nix build github:vicondoa/virgl-vaapi-compat#firefox-virgl-vaapi
nix build github:vicondoa/virgl-vaapi-compat#compatibility-report
```

See [`docs/nix.md`](docs/nix.md).

Firefox users should treat this as one possible workaround for a VA-API DMABUF
format-descriptor mismatch, not as a general Firefox acceleration switch. You
still need a working VA-API virtio-gpu stack and Firefox VA-API enabled. See
[`docs/firefox.md`](docs/firefox.md) and [`docs/debugging.md`](docs/debugging.md).

## Safety and scope

`virgl-vaapi-compat` intentionally has a small blast radius:

- hooks only `vaExportSurfaceHandle`
- rewrites only successful `DRM_PRIME_2` I420 surface exports with at least the
  U and V layer entries needed for the swap
- changes descriptor metadata only; it does not touch pixel data or file
  descriptors
- leaves unsupported formats and failure paths untouched
- can be enabled per-process with `LIBVA_DRIVERS_PATH`

Because it shadows a system VA driver name, deploy it conservatively: keep the
real driver available, test with `vainfo` and a known VA-API client, and remove
it once the underlying stack no longer needs descriptor translation.

## Status

This project is a focused compatibility adapter for a currently observed
virgl/virtio-gpu VA-API interop issue. It should become unnecessary when either
virgl/Mesa exports a client-accepted DRM PRIME descriptor directly, or affected
clients accept the existing virgl I420 descriptor path. See
[`docs/removal-criteria.md`](docs/removal-criteria.md).

## Documentation

- [`docs/how-it-works.md`](docs/how-it-works.md) - operational walkthrough of
  the shim
- [`docs/design.md`](docs/design.md) - design goals, non-goals, and constraints
- [`docs/debugging.md`](docs/debugging.md) - reproducing and diagnosing the
  motivating failure mode
- [`docs/firefox.md`](docs/firefox.md) - Firefox-specific VA-API notes
- [`docs/nix.md`](docs/nix.md) - Nix/NixOS integration notes
- [`docs/troubleshooting.md`](docs/troubleshooting.md) - common symptoms and
  fixes
- [`docs/removal-criteria.md`](docs/removal-criteria.md) - how to know when the
  shim can be retired

## License

Apache-2.0. See [`LICENSE`](LICENSE).
