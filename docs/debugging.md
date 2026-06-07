# Debugging virgl VA-API descriptor issues

This document describes how to debug the class of issue that motivated
`virgl-vaapi-compat`: VA-API decode is selected, but decoded surface export to
DMABUF fails because the client rejects the exported DRM PRIME descriptor shape.

## Known motivating investigation

A prior debugging session (`f15bd57d-e724-47f2-9d2e-012652265f5e`) found:

- The original motivating browser client used VA-API for this hardware decode
  path rather than the `virtio_media` `/dev/video0` V4L2 M2M path directly.
- Enabling `graphics.virglVideo = true` made the guest virtio-gpu VA-API driver
  expose H.264 profiles.
- The client selected H.264 VA-API decode but rejected the exported DMABUF
  descriptor, then fell back to software decode.
- The root cause was virgl exporting `DRM_PRIME_2` `VA_FOURCC_I420` descriptors
  while the client rejected that descriptor before reaching its lower DMABUF YUV
  import path.
- A first workaround patched the client allowlist. This project is the preferred
  wrapper/shim approach because it leaves clients unpatched.

Known stack versions from that investigation:

| Component | Version / revision |
| --- | --- |
| nixpkgs | `331800de5053fcebacf6813adb5db9c9dca22a0c` |
| libva | `2.23.0` (`__vaDriverInit_1_23`) |
| Mesa | `26.1.1` |
| virglrenderer | `1.3.0` |
| crosvm | `4c80bf3523cf84114054209d88a7af3eefd8423f` |
| Cloud Hypervisor | `52.0` |
## Establish what path the client uses

First separate three different acceleration paths:

1. **V4L2 M2M** such as `virtio_media` and `/dev/video0`
2. **VA-API** through libva and `virtio_gpu_drv_video.so`
3. **Software decode**

For the motivating browser client, the relevant hardware decode path was VA-API.
Seeing a `virtio_media` node was not enough because that client did not directly
use the V4L2 M2M path for this class of decode.

## Check VA-API visibility

Inside the guest, verify that libva can load the virtio-gpu VA driver and that
H.264 profiles are advertised:

```bash
LIBVA_DRIVER_NAME=virtio_gpu vainfo
```

Useful variants:

```bash
LIBVA_MESSAGING_LEVEL=2 LIBVA_DRIVER_NAME=virtio_gpu vainfo
LIBVA_TRACE=va.trace LIBVA_DRIVER_NAME=virtio_gpu vainfo
```

Look for:

- the driver name/path libva loaded
- the libva version and backend ABI
- H.264 decode profiles such as `VAProfileH264Main` or `VAProfileH264High`
- `VAEntrypointVLD`

If H.264 profiles are absent, fix the virtio-gpu/virgl video stack first. The
shim cannot create decoder capabilities.

## Check the libva driver ABI

The shim exports a libva init symbol derived at build time. For libva 2.23.x,
the expected symbol is:

```text
__vaDriverInit_1_23
```

If libva cannot initialize the shim, confirm that the build-time libva headers
and runtime libva agree. Rebuild with the target system's `pkg-config` and libva
headers when in doubt.

## DMABUF and I420 failure signature

The failure this shim targets has a specific shape:

1. VA-API decode is selected.
2. The virtio-gpu VA driver successfully decodes or prepares surfaces.
3. The client requests `vaExportSurfaceHandle()` with `DRM_PRIME_2`.
4. The exported descriptor is planar `VA_FOURCC_I420` with separate Y, U, and V
   layer entries.
5. The client rejects the descriptor before its lower DMABUF YUV import path.
6. Playback falls back to software decode.

If the client never selects VA-API, if the driver cannot export DMABUFs at all,
or if another format is failing, this shim may not help.

## Debug with the shim

Enable shim logging per process:

```bash
VIRGL_VAAPI_COMPAT_DEBUG=1 \
LIBVA_DRIVER_NAME=virtio_gpu \
LIBVA_DRIVERS_PATH=/path/to/shim/lib/dri${LIBVA_DRIVERS_PATH:+:$LIBVA_DRIVERS_PATH} \
vainfo
```

For an affected client, include the same shim variables in the launch
environment. When the hook is installed and a matching export occurs, stderr may
include lines like:

```text
virgl-vaapi-compat: installed VA export descriptor compatibility hook
virgl-vaapi-compat: exported surface fourcc=0x30323449 layers=3 objects=...
virgl-vaapi-compat: rewrote I420 descriptor to YV12 with U/V layer swap
```

If you only see initialization messages and no export messages, the client may
not be reaching `vaExportSurfaceHandle()`, may be using another driver, or may
not be decoding with VA-API.

If you see I420 exports left unchanged because too few layers were present, the
descriptor does not match the known safe translation case and should be analyzed
before extending the shim.

## Compare with and without the shim

Always capture both sides:

```bash
# Baseline
LIBVA_DRIVER_NAME=virtio_gpu app

# With descriptor shim
VIRGL_VAAPI_COMPAT_DEBUG=1 \
LIBVA_DRIVER_NAME=virtio_gpu \
LIBVA_DRIVERS_PATH=/path/to/shim/lib/dri${LIBVA_DRIVERS_PATH:+:$LIBVA_DRIVERS_PATH} \
app
```

A successful result for this project is not merely that playback works. The
important evidence is:

- VA-API is selected in both cases
- the baseline fails at DMABUF/image/surface export
- the shim sees and rewrites the I420 `DRM_PRIME_2` descriptor
- the client continues on the hardware decode path after the rewrite
