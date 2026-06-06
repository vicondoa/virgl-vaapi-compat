# Firefox notes

Firefox is the original motivating client for this project, but
`virgl-vaapi-compat` is not Firefox-specific. The shim sits at the libva driver
boundary and can be used by any VA-API client that loads the virtio-gpu VA
driver.

## What Firefox needs first

Before testing the shim, confirm the guest already has a working VA-API path:

```bash
LIBVA_DRIVER_NAME=virtio_gpu vainfo
```

The output should show H.264 decode profiles and `VAEntrypointVLD`. If those are
missing, fix the VM/guest graphics stack first. In the motivating environment,
turning on virgl video support exposed H.264 profiles; the shim did not create
those capabilities.

Firefox on Linux uses VA-API for this class of hardware decode. A
`virtio_media` `/dev/video0` V4L2 M2M device is not the path Firefox used in the
investigation that led to this shim.

## Launching Firefox with the shim

Example launch:

```bash
VIRGL_VAAPI_COMPAT_DEBUG=1 \
LIBVA_DRIVER_NAME=virtio_gpu \
LIBVA_DRIVERS_PATH=/path/to/shim/lib/dri${LIBVA_DRIVERS_PATH:+:$LIBVA_DRIVERS_PATH} \
MOZ_LOG="PlatformDecoderModule:5,FFmpegVideo:5,DMABUF:5,WidgetDMABuf:5,VAAPI:5" \
MOZ_LOG_FILE=firefox-vaapi.log \
firefox
```

Depending on the distribution and Firefox build, other Firefox preferences or
feature flags may be required for VA-API. This repository does not try to be a
complete Firefox acceleration guide; it addresses the narrower descriptor export
compatibility problem after VA-API decode is already being selected.

## Expected evidence

In Firefox logs, look for evidence that H.264 VA-API decode was selected, such
as:

- `hw: "true"`
- `VAAPI_VLD`

In failing baseline logs for the motivating bug, relevant messages included:

- `CreateImageVAAPI(): failed to get VideoFrameSurface`
- `VAAPI dmabuf allocation error`

With the shim and `VIRGL_VAAPI_COMPAT_DEBUG=1`, stderr should show the shim hook
being installed and, during playback, matching I420 `DRM_PRIME_2` exports being
rewritten to YV12 with a U/V layer swap.

## What this does not fix

The shim will not help if:

- Firefox is using software decode from the start
- Firefox is blocked by sandbox, Wayland, EGL, or GPU process issues unrelated
  to VA-API surface export
- `vainfo` does not show the needed decode profile
- the VM has no working virgl/virtio-gpu video path
- the failure is for a different fourcc, memory type, or descriptor layout

When in doubt, follow [`debugging.md`](debugging.md) and compare logs with and
without the shim.
