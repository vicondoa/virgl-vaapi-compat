#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <va/va.h>
#include <va/va_backend.h>
#include <va/va_drmcommon.h>

#include "core-fake-driver.h"

#ifndef VIRGL_VAAPI_COMPAT_ABI_MAJOR
#define VIRGL_VAAPI_COMPAT_ABI_MAJOR 1
#endif

#ifndef VIRGL_VAAPI_COMPAT_ABI_MINOR
#error "VIRGL_VAAPI_COMPAT_ABI_MINOR must be defined"
#endif

#define VVC_INIT_SYMBOL_(major, minor) __vaDriverInit_##major##_##minor
#define VVC_INIT_SYMBOL(major, minor) VVC_INIT_SYMBOL_(major, minor)

typedef VAStatus (*ExportSurfaceHandleFn)(
    VADriverContextP ctx,
    VASurfaceID surface_id,
    uint32_t mem_type,
    uint32_t flags,
    void *descriptor);

static unsigned int init_calls;
static unsigned int export_calls;

static void fill_layer(
    VADRMPRIMESurfaceDescriptor *desc,
    unsigned int layer,
    uint32_t drm_format,
    uint32_t object_index,
    uint32_t offset,
    uint32_t pitch) {
  desc->layers[layer].drm_format = drm_format;
  desc->layers[layer].num_planes = 1;
  desc->layers[layer].object_index[0] = object_index;
  desc->layers[layer].offset[0] = offset;
  desc->layers[layer].pitch[0] = pitch;
}

static void fill_i420_descriptor(
    VADRMPRIMESurfaceDescriptor *desc,
    uint32_t num_layers) {
  memset(desc, 0, sizeof(*desc));
  desc->fourcc = VA_FOURCC_I420;
  desc->width = 640;
  desc->height = 480;
  desc->num_objects = 3;
  desc->num_layers = num_layers;

  fill_layer(desc,
             0,
             VVC_FAKE_LAYER_Y_FORMAT,
             VVC_FAKE_LAYER_Y_OBJECT,
             VVC_FAKE_LAYER_Y_OFFSET,
             VVC_FAKE_LAYER_Y_PITCH);
  fill_layer(desc,
             1,
             VVC_FAKE_LAYER_U_FORMAT,
             VVC_FAKE_LAYER_U_OBJECT,
             VVC_FAKE_LAYER_U_OFFSET,
             VVC_FAKE_LAYER_U_PITCH);
  fill_layer(desc,
             2,
             VVC_FAKE_LAYER_V_FORMAT,
             VVC_FAKE_LAYER_V_OBJECT,
             VVC_FAKE_LAYER_V_OFFSET,
             VVC_FAKE_LAYER_V_PITCH);
}

static VAStatus fake_export_surface_handle(
    VADriverContextP ctx,
    VASurfaceID surface_id,
    uint32_t mem_type,
    uint32_t flags,
    void *descriptor) {
  (void)ctx;
  (void)mem_type;
  (void)flags;
  export_calls++;

  if (!descriptor) {
    return VA_STATUS_SUCCESS;
  }

  VADRMPRIMESurfaceDescriptor *desc =
      (VADRMPRIMESurfaceDescriptor *)descriptor;

  if (surface_id == VVC_FAKE_SURFACE_I420_TWO_LAYERS) {
    fill_i420_descriptor(desc, 2);
    return VA_STATUS_SUCCESS;
  }

  fill_i420_descriptor(desc, 3);
  if (surface_id == VVC_FAKE_SURFACE_FAILING_I420_THREE_LAYERS) {
    return VVC_FAKE_FAILURE_STATUS;
  }

  return VA_STATUS_SUCCESS;
}

void virgl_vaapi_compat_test_fake_reset(void) {
  init_calls = 0;
  export_calls = 0;
}

unsigned int virgl_vaapi_compat_test_fake_init_calls(void) {
  return init_calls;
}

unsigned int virgl_vaapi_compat_test_fake_export_calls(void) {
  return export_calls;
}

ExportSurfaceHandleFn virgl_vaapi_compat_test_fake_export_function(void) {
  return fake_export_surface_handle;
}

VAStatus VVC_INIT_SYMBOL(VIRGL_VAAPI_COMPAT_ABI_MAJOR,
                         VIRGL_VAAPI_COMPAT_ABI_MINOR)(VADriverContextP ctx) {
  init_calls++;
  const char *fail_init = getenv(VVC_FAKE_FAIL_INIT_ENV);
  if (fail_init && strcmp(fail_init, "1") == 0) {
    return VVC_FAKE_FAILURE_STATUS;
  }

  ctx->vtable->vaExportSurfaceHandle = fake_export_surface_handle;
  return VA_STATUS_SUCCESS;
}
