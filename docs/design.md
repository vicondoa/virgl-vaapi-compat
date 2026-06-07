# Design

This project is deliberately narrow. It translates one known VA-API export
descriptor shape from the virtio-gpu/virgl video stack into an equivalent shape
accepted by affected clients.

## Goals

- Keep the core C shim independent of any specific client.
- Keep the shim independent of Nix, NixOS, and any particular distribution.
- Delegate decode, allocation, synchronization, and device behavior to the real
  `virtio_gpu_drv_video.so` driver.
- Hook exactly one VA-API operation: `vaExportSurfaceHandle`.
- Rewrite only successful `DRM_PRIME_2` I420 descriptors that match the known
  planar YUV compatibility issue.
- Make the behavior inspectable through opt-in debug logging.
- Make the workaround easy to remove when the underlying stack is fixed.

## Non-goals

`virgl-vaapi-compat` is not:

- a VA-API implementation
- a video decoder
- a pixel format converter
- a color management tool
- a DMABUF allocator
- a GPU synchronization layer
- a Mesa, virglrenderer, crosvm, browser, or client fork
- a generic workaround for all virtio-gpu graphics problems

If a client fails before VA-API decode is selected, if H.264 profiles are not
advertised, if DMABUF import fails for unrelated reasons, or if the guest cannot
use virtio-gpu video at all, this shim is not the primary fix.

## Why a shim

The motivating failure was not that H.264 decode was unavailable. The guest
virtio-gpu VA driver advertised H.264 decode and the affected client selected
VA-API, but surface export failed at a descriptor compatibility boundary: virgl
exported a `DRM_PRIME_2` I420 descriptor, and the client rejected that descriptor
before falling through to its lower DMABUF YUV import path.

A client patch that allowed that I420 descriptor can work, but it ties the
workaround to one client. A VA driver shim keeps clients unpatched and confines
the workaround to the driver boundary where the descriptor is produced.

## Descriptor translation

For planar 4:2:0 YUV, I420 and YV12 differ in U/V plane order. The shim handles
the known virgl export by changing the reported fourcc from `VA_FOURCC_I420` to
`VA_FOURCC_YV12` and swapping the U and V layer descriptors.

This preserves the interpretation of the underlying planes for clients that
accept YV12 but reject the I420 descriptor path. It does not copy or transform
pixels.

## Failure model

The shim should fail closed:

- If the real driver cannot be loaded, initialization fails loudly.
- If the real driver lacks the requested libva init symbol, initialization fails
  loudly.
- If the real driver returns an export error, the error is returned unchanged.
- If an export does not match the compatibility case, the descriptor is returned
  unchanged.

The safest deployment model is per-process activation through
`LIBVA_DRIVERS_PATH`, with the real driver kept at a separate absolute path.
