# How it works

`virgl-vaapi-compat` is a VA-API driver shim. It is built as a shared object
named `virtio_gpu_drv_video.so` so libva can load it when a process selects the
`virtio_gpu` driver from a directory placed early in `LIBVA_DRIVERS_PATH`.

The shim then loads the real virtio-gpu VA driver and delegates almost
everything to it.

## Load path

A typical launch looks like this:

```bash
LIBVA_DRIVER_NAME=virtio_gpu \
LIBVA_DRIVERS_PATH=/path/to/shim/lib/dri${LIBVA_DRIVERS_PATH:+:$LIBVA_DRIVERS_PATH} \
app-that-uses-vaapi
```

libva searches `LIBVA_DRIVERS_PATH` for `virtio_gpu_drv_video.so` and finds the
shim first. During driver initialization the shim:

1. `dlopen()`s the real driver path compiled in with `REAL_DRIVER=...`.
2. Finds the matching libva initialization symbol, for example
   `__vaDriverInit_1_23` for libva 2.23.x.
3. Calls the real driver's initialization function with the original
   `VADriverContextP`.
4. Saves the real `vaExportSurfaceHandle` callback.
5. Replaces only `ctx->vtable->vaExportSurfaceHandle` with the shim callback.

After initialization, all driver state and all unmodified callbacks still come
from the real driver.

## Export hook

The only runtime interception is `vaExportSurfaceHandle()`.

The shim calls the real driver's export function first. It inspects the returned
descriptor only when all of these are true:

- the real export returned `VA_STATUS_SUCCESS`
- the requested memory type is `VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2`
- the caller supplied a non-null descriptor pointer
- the descriptor reports `VA_FOURCC_I420`
- the descriptor has enough layer entries for the U/V layer swap; the motivating
  virgl export is a three-plane descriptor

For that descriptor, the shim:

1. changes `desc->fourcc` from `VA_FOURCC_I420` to `VA_FOURCC_YV12`
2. swaps `desc->layers[1]` and `desc->layers[2]`

No object file descriptors, offsets, pitches, modifiers, image bytes, decoder
state, or GPU resources are rewritten. The change is metadata-level descriptor
translation for two equivalent planar 4:2:0 layouts whose U/V layer order is the
client compatibility boundary.

## Pass-through behavior

The shim leaves all other cases untouched, including:

- failed exports
- non-`DRM_PRIME_2` exports
- non-I420 formats
- descriptors without the needed layer entries
- all other VA-API functions

If debug logging is enabled with `VIRGL_VAAPI_COMPAT_DEBUG=1`, the shim prints
what it saw and whether it rewrote the descriptor. Without that environment
variable it is quiet except for initialization errors such as failing to load the
real driver.

## ABI symbol

libva driver entry points include the libva backend ABI in their symbol name,
such as `__vaDriverInit_1_23`. The Makefile derives the minor version from
the `VA_MAJOR_VERSION` and `VA_MINOR_VERSION` macros in the active libva headers
and uses `pkg-config` only to find those headers and libraries. It then compiles
the corresponding symbol into the shim. If you build against a different libva
than the runtime expects, libva may fail to initialize the shim.
