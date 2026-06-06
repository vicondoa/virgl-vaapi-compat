#include <stddef.h>
#include <stdint.h>

#include <va/va.h>
#include <va/va_backend.h>
#include <va/va_drmcommon.h>
#include <va/va_version.h>

#ifndef VA_MAJOR_VERSION
#error "libva headers do not expose VA_MAJOR_VERSION"
#endif

#ifndef VA_MINOR_VERSION
#error "libva headers do not expose VA_MINOR_VERSION"
#endif

#ifndef VA_STATUS_SUCCESS
#error "libva headers do not expose VA_STATUS_SUCCESS"
#endif

#ifndef VA_STATUS_ERROR_UNKNOWN
#error "libva headers do not expose VA_STATUS_ERROR_UNKNOWN"
#endif

#ifndef VA_STATUS_ERROR_OPERATION_FAILED
#error "libva headers do not expose VA_STATUS_ERROR_OPERATION_FAILED"
#endif

#ifndef VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2
#error "libva headers do not expose VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2"
#endif

#ifndef VA_FOURCC_I420
#error "libva headers do not expose VA_FOURCC_I420"
#endif

#ifndef VA_FOURCC_YV12
#error "libva headers do not expose VA_FOURCC_YV12"
#endif

#define VVC_FIELD_SIZE(type, field) sizeof(((type *)0)->field)
#define VVC_ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

typedef VAStatus (*ExportSurfaceHandleFn)(
    VADriverContextP ctx,
    VASurfaceID surface_id,
    uint32_t mem_type,
    uint32_t flags,
    void *descriptor);

_Static_assert(VA_MAJOR_VERSION >= 0, "VA_MAJOR_VERSION must be numeric");
_Static_assert(VA_MINOR_VERSION >= 0, "VA_MINOR_VERSION must be numeric");
_Static_assert(VA_FOURCC_I420 != VA_FOURCC_YV12,
               "I420 and YV12 must be distinct formats");
_Static_assert(VVC_FIELD_SIZE(VADRMPRIMESurfaceDescriptor, fourcc) ==
                   sizeof(uint32_t),
               "VADRMPRIMESurfaceDescriptor.fourcc is required");
_Static_assert(VVC_FIELD_SIZE(VADRMPRIMESurfaceDescriptor, num_objects) ==
                   sizeof(uint32_t),
               "VADRMPRIMESurfaceDescriptor.num_objects is required");
_Static_assert(VVC_FIELD_SIZE(VADRMPRIMESurfaceDescriptor, num_layers) ==
                   sizeof(uint32_t),
               "VADRMPRIMESurfaceDescriptor.num_layers is required");
_Static_assert(VVC_ARRAY_LEN(((VADRMPRIMESurfaceDescriptor *)0)->objects) >= 1,
               "VADRMPRIMESurfaceDescriptor.objects is required");
_Static_assert(VVC_ARRAY_LEN(((VADRMPRIMESurfaceDescriptor *)0)->layers) >= 3,
               "VADRMPRIMESurfaceDescriptor.layers needs at least 3 entries");
_Static_assert(
    VVC_FIELD_SIZE(VADRMPRIMESurfaceDescriptor, layers[0].drm_format) ==
        sizeof(uint32_t),
    "VADRMPRIMESurfaceDescriptor.layers[].drm_format is required");
_Static_assert(VVC_FIELD_SIZE(VADRMPRIMESurfaceDescriptor,
                              layers[0].object_index[0]) == sizeof(uint32_t),
               "VADRMPRIMESurfaceDescriptor.layers[].object_index[] is required");
_Static_assert(VVC_FIELD_SIZE(VADRMPRIMESurfaceDescriptor, layers[0].offset[0]) ==
                   sizeof(uint32_t),
               "VADRMPRIMESurfaceDescriptor.layers[].offset[] is required");
_Static_assert(VVC_FIELD_SIZE(VADRMPRIMESurfaceDescriptor, layers[0].pitch[0]) ==
                   sizeof(uint32_t),
               "VADRMPRIMESurfaceDescriptor.layers[].pitch[] is required");
_Static_assert(VVC_FIELD_SIZE(struct VADriverContext, vtable) ==
                   sizeof(struct VADriverVTable *),
               "VADriverContext.vtable is required");
_Static_assert(VVC_FIELD_SIZE(struct VADriverVTable, vaExportSurfaceHandle) > 0,
               "VADriverVTable.vaExportSurfaceHandle is required");

int virgl_vaapi_compat_probe_typecheck(VADriverContextP ctx) {
  ExportSurfaceHandleFn fn;

  fn = ctx->vtable->vaExportSurfaceHandle;
  (void)fn;
  (void)offsetof(VADRMPRIMESurfaceDescriptor, layers);
  return 0;
}
