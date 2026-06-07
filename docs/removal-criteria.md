# Removal criteria

`virgl-vaapi-compat` is a workaround. The best long-term outcome is to remove
it because the underlying driver/client combination no longer needs descriptor
translation.

## Remove the shim when any of these are true

- virgl/Mesa exports a DRM PRIME descriptor shape that affected clients accept
  directly
- affected clients accept the current virtio-gpu I420 `DRM_PRIME_2` descriptor
  path without patching or wrapping
- the workload no longer uses the virtio-gpu VA driver path
- the relevant guests move to another working decode/export path
- testing shows the shim no longer rewrites descriptors during normal playback

## Validation before removal

Compare the same workload with and without the shim:

1. Run `vainfo` with the normal virtio-gpu VA driver and confirm decode profiles
   are still present.
2. Run the affected client without `LIBVA_DRIVERS_PATH` pointing to the shim.
3. Confirm the client selects VA-API hardware decode.
4. Confirm playback continues without DMABUF/image allocation failures.
5. Confirm there are no regressions in color, stability, or fallback behavior.

Useful evidence includes client logs showing hardware decode selected, no
recurrence of the original DMABUF/image allocation failure, and no shim debug
messages indicating descriptor rewrites during normal playback.

## Keep the test harness

Even after removing the shim from a deployment, the repository's harness remains
useful. It verifies the intended transformation in isolation and makes future
changes explicit if a similar descriptor compatibility issue appears again.
