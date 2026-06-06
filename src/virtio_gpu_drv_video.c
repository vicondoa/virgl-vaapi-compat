#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <va/va.h>
#include <va/va_backend.h>
#include <va/va_drmcommon.h>

#ifndef VIRGL_VAAPI_COMPAT_REAL_DRIVER
#define VIRGL_VAAPI_COMPAT_REAL_DRIVER "/run/opengl-driver/lib/dri/virtio_gpu_drv_video.so"
#endif

#ifndef VIRGL_VAAPI_COMPAT_ABI_MAJOR
#define VIRGL_VAAPI_COMPAT_ABI_MAJOR 1
#endif

#ifndef VIRGL_VAAPI_COMPAT_ABI_MINOR
#error "VIRGL_VAAPI_COMPAT_ABI_MINOR must be defined, for example -DVIRGL_VAAPI_COMPAT_ABI_MINOR=23"
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

#ifndef VA_STATUS_SUCCESS
#error "libva headers do not expose VA_STATUS_SUCCESS"
#endif

#ifndef VA_STATUS_ERROR_UNKNOWN
#error "libva headers do not expose VA_STATUS_ERROR_UNKNOWN"
#endif

#define VVC_INIT_SYMBOL_(major, minor) __vaDriverInit_##major##_##minor
#define VVC_INIT_SYMBOL(major, minor) VVC_INIT_SYMBOL_(major, minor)
#define VVC_INIT_SYMBOL_STRING_(major, minor) "__vaDriverInit_" #major "_" #minor
#define VVC_INIT_SYMBOL_STRING(major, minor) VVC_INIT_SYMBOL_STRING_(major, minor)

#define VVC_FIELD_SIZE(type, field) sizeof(((type *)0)->field)
#define VVC_ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

_Static_assert(VVC_FIELD_SIZE(VADRMPRIMESurfaceDescriptor, fourcc) ==
                   sizeof(uint32_t),
               "VADRMPRIMESurfaceDescriptor.fourcc is required");
_Static_assert(VVC_FIELD_SIZE(VADRMPRIMESurfaceDescriptor, num_objects) ==
                   sizeof(uint32_t),
               "VADRMPRIMESurfaceDescriptor.num_objects is required");
_Static_assert(VVC_FIELD_SIZE(VADRMPRIMESurfaceDescriptor, num_layers) ==
                   sizeof(uint32_t),
               "VADRMPRIMESurfaceDescriptor.num_layers is required");
_Static_assert(VVC_ARRAY_LEN(((VADRMPRIMESurfaceDescriptor *)0)->layers) >= 3,
               "VADRMPRIMESurfaceDescriptor.layers is required");
_Static_assert(
    VVC_FIELD_SIZE(VADRMPRIMESurfaceDescriptor, layers[0].drm_format) ==
        sizeof(uint32_t),
    "VADRMPRIMESurfaceDescriptor.layers[].drm_format is required");
_Static_assert(VVC_FIELD_SIZE(struct VADriverContext, vtable) ==
                   sizeof(struct VADriverVTable *),
               "VADriverContext.vtable is required");
_Static_assert(VVC_FIELD_SIZE(struct VADriverVTable, vaExportSurfaceHandle) > 0,
               "VADriverVTable.vaExportSurfaceHandle is required");

typedef VAStatus (*ExportSurfaceHandleFn)(
    VADriverContextP ctx,
    VASurfaceID surface_id,
    uint32_t mem_type,
    uint32_t flags,
    void *descriptor);

static ExportSurfaceHandleFn real_export_surface_handle;

static void swap_prime_layers(
    VADRMPRIMESurfaceDescriptor *desc,
    unsigned int a,
    unsigned int b) {
  unsigned char tmp[sizeof(desc->layers[0])];
  memcpy(tmp, &desc->layers[a], sizeof(tmp));
  memcpy(&desc->layers[a], &desc->layers[b], sizeof(desc->layers[a]));
  memcpy(&desc->layers[b], tmp, sizeof(desc->layers[b]));
}

static int debug_enabled(void) {
  const char *value = getenv("VIRGL_VAAPI_COMPAT_DEBUG");
  return value && strcmp(value, "1") == 0;
}

static void debugf(const char *fmt, ...) {
  if (!debug_enabled()) {
    return;
  }

  va_list ap;
  va_start(ap, fmt);
  fputs("virgl-vaapi-compat: ", stderr);
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  va_end(ap);
}

static VAStatus compat_export_surface_handle(
    VADriverContextP ctx,
    VASurfaceID surface_id,
    uint32_t mem_type,
    uint32_t flags,
    void *descriptor) {
  if (!real_export_surface_handle) {
    return VA_STATUS_ERROR_UNKNOWN;
  }

  VAStatus status = real_export_surface_handle(
      ctx, surface_id, mem_type, flags, descriptor);

  if (status == VA_STATUS_SUCCESS &&
      mem_type == VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2 &&
      descriptor) {
    VADRMPRIMESurfaceDescriptor *desc =
        (VADRMPRIMESurfaceDescriptor *)descriptor;

    debugf("exported surface fourcc=0x%08x layers=%u objects=%u",
           desc->fourcc, desc->num_layers, desc->num_objects);

    if (desc->fourcc == VA_FOURCC_I420 && desc->num_layers >= 3) {
      desc->fourcc = VA_FOURCC_YV12;
      swap_prime_layers(desc, 1, 2);
      debugf("rewrote I420 descriptor to YV12 with U/V layer swap");
    } else if (desc->fourcc == VA_FOURCC_I420) {
      debugf("left I420 descriptor unchanged: expected at least 3 layers");
    }
  }

  return status;
}

static VAStatus init_real_driver(const char *symbol, VADriverContextP ctx) {
  if (VIRGL_VAAPI_COMPAT_REAL_DRIVER[0] != '/') {
    fprintf(stderr,
            "virgl-vaapi-compat: real driver path must be absolute: %s\n",
            VIRGL_VAAPI_COMPAT_REAL_DRIVER);
    return VA_STATUS_ERROR_UNKNOWN;
  }

  void *real_driver = dlopen(
      VIRGL_VAAPI_COMPAT_REAL_DRIVER,
      RTLD_NOW | RTLD_LOCAL);
  if (!real_driver) {
    fprintf(stderr,
            "virgl-vaapi-compat: dlopen real virtio_gpu VA driver failed: %s\n",
            dlerror());
    return VA_STATUS_ERROR_UNKNOWN;
  }

  typedef VAStatus (*InitFn)(VADriverContextP);
  InitFn init = (InitFn)dlsym(real_driver, symbol);
  if (!init) {
    fprintf(stderr,
            "virgl-vaapi-compat: missing real driver symbol %s: %s\n",
            symbol,
            dlerror());
    dlclose(real_driver);
    return VA_STATUS_ERROR_UNKNOWN;
  }

  VAStatus status = init(ctx);
  if (status != VA_STATUS_SUCCESS) {
    dlclose(real_driver);
    return status;
  }
  if (!ctx || !ctx->vtable || !ctx->vtable->vaExportSurfaceHandle) {
    fprintf(stderr,
            "virgl-vaapi-compat: real driver did not expose vaExportSurfaceHandle\n");
    dlclose(real_driver);
    return VA_STATUS_ERROR_UNKNOWN;
  }

  real_export_surface_handle = ctx->vtable->vaExportSurfaceHandle;
  ctx->vtable->vaExportSurfaceHandle = compat_export_surface_handle;
  debugf("installed VA export descriptor compatibility hook");
  return status;
}

VAStatus VVC_INIT_SYMBOL(VIRGL_VAAPI_COMPAT_ABI_MAJOR,
                         VIRGL_VAAPI_COMPAT_ABI_MINOR)(VADriverContextP ctx) {
  return init_real_driver(
      VVC_INIT_SYMBOL_STRING(VIRGL_VAAPI_COMPAT_ABI_MAJOR,
                             VIRGL_VAAPI_COMPAT_ABI_MINOR),
      ctx);
}
