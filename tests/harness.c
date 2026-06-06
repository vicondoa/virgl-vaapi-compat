#define _POSIX_C_SOURCE 200112L

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <va/va.h>
#include <va/va_backend.h>
#include <va/va_drmcommon.h>
#include <va/va_version.h>

#include "core-fake-driver.h"

#define VVC_STR_(value) #value
#define VVC_STR(value) VVC_STR_(value)
#define VVC_DERIVED_INIT_SYMBOL \
  "__vaDriverInit_" VVC_STR(VA_MAJOR_VERSION) "_" VVC_STR(VA_MINOR_VERSION)

typedef VAStatus (*InitFn)(VADriverContextP);
typedef VAStatus (*ExportSurfaceHandleFn)(
    VADriverContextP ctx,
    VASurfaceID surface_id,
    uint32_t mem_type,
    uint32_t flags,
    void *descriptor);
typedef void (*FakeResetFn)(void);
typedef unsigned int (*FakeCounterFn)(void);
typedef ExportSurfaceHandleFn (*FakeExportFunctionFn)(void);

static int fail(const char *message) {
  fputs(message, stderr);
  fputc('\n', stderr);
  return 1;
}

static void *load_symbol(void *handle, const char *name) {
  dlerror();
  void *symbol = dlsym(handle, name);
  const char *error = dlerror();
  if (error) {
    fputs(error, stderr);
    fputc('\n', stderr);
    return NULL;
  }

  return symbol;
}

static int expect_status(
    const char *label,
    VAStatus actual,
    VAStatus expected) {
  if (actual == expected) {
    return 0;
  }

  fprintf(stderr,
          "%s: expected VA status 0x%08x, got 0x%08x\n",
          label,
          expected,
          actual);
  return 1;
}

static int expect_u32(
    const char *label,
    uint32_t actual,
    uint32_t expected) {
  if (actual == expected) {
    return 0;
  }

  fprintf(stderr,
          "%s: expected 0x%08x, got 0x%08x\n",
          label,
          expected,
          actual);
  return 1;
}

static int expect_counter(
    const char *label,
    unsigned int actual,
    unsigned int expected) {
  if (actual == expected) {
    return 0;
  }

  fprintf(stderr,
          "%s: expected %u, got %u\n",
          label,
          expected,
          actual);
  return 1;
}

static int expect_layer(
    const char *label,
    const VADRMPRIMESurfaceDescriptor *desc,
    unsigned int layer,
    uint32_t drm_format,
    uint32_t object_index,
    uint32_t offset,
    uint32_t pitch) {
  char field[128];

  snprintf(field, sizeof(field), "%s drm_format", label);
  if (expect_u32(field, desc->layers[layer].drm_format, drm_format)) {
    return 1;
  }
  snprintf(field, sizeof(field), "%s object_index", label);
  if (expect_u32(field, desc->layers[layer].object_index[0], object_index)) {
    return 1;
  }
  snprintf(field, sizeof(field), "%s offset", label);
  if (expect_u32(field, desc->layers[layer].offset[0], offset)) {
    return 1;
  }
  snprintf(field, sizeof(field), "%s pitch", label);
  return expect_u32(field, desc->layers[layer].pitch[0], pitch);
}

static int expect_original_uv_layers(
    const char *label,
    const VADRMPRIMESurfaceDescriptor *desc) {
  char layer_label[128];

  snprintf(layer_label, sizeof(layer_label), "%s U layer", label);
  if (expect_layer(layer_label,
                   desc,
                   1,
                   VVC_FAKE_LAYER_U_FORMAT,
                   VVC_FAKE_LAYER_U_OBJECT,
                   VVC_FAKE_LAYER_U_OFFSET,
                   VVC_FAKE_LAYER_U_PITCH)) {
    return 1;
  }

  snprintf(layer_label, sizeof(layer_label), "%s V layer", label);
  return expect_layer(layer_label,
                      desc,
                      2,
                      VVC_FAKE_LAYER_V_FORMAT,
                      VVC_FAKE_LAYER_V_OBJECT,
                      VVC_FAKE_LAYER_V_OFFSET,
                      VVC_FAKE_LAYER_V_PITCH);
}

static int expect_swapped_uv_layers(
    const char *label,
    const VADRMPRIMESurfaceDescriptor *desc) {
  char layer_label[128];

  snprintf(layer_label, sizeof(layer_label), "%s swapped V layer", label);
  if (expect_layer(layer_label,
                   desc,
                   1,
                   VVC_FAKE_LAYER_V_FORMAT,
                   VVC_FAKE_LAYER_V_OBJECT,
                   VVC_FAKE_LAYER_V_OFFSET,
                   VVC_FAKE_LAYER_V_PITCH)) {
    return 1;
  }

  snprintf(layer_label, sizeof(layer_label), "%s swapped U layer", label);
  return expect_layer(layer_label,
                      desc,
                      2,
                      VVC_FAKE_LAYER_U_FORMAT,
                      VVC_FAKE_LAYER_U_OBJECT,
                      VVC_FAKE_LAYER_U_OFFSET,
                      VVC_FAKE_LAYER_U_PITCH);
}

static int test_drm_prime_i420_rewrite(
    ExportSurfaceHandleFn export_surface,
    VADriverContextP ctx,
    FakeCounterFn export_calls) {
  VADRMPRIMESurfaceDescriptor desc;
  memset(&desc, 0xa5, sizeof(desc));

  VAStatus status = export_surface(ctx,
                                   VVC_FAKE_SURFACE_I420_THREE_LAYERS,
                                   VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                   0,
                                   &desc);
  if (expect_status("DRM_PRIME_2 I420 export", status, VA_STATUS_SUCCESS) ||
      expect_counter("fake export calls after rewrite test",
                     export_calls(),
                     1) ||
      expect_u32("DRM_PRIME_2 I420 fourcc", desc.fourcc, VA_FOURCC_YV12) ||
      expect_u32("DRM_PRIME_2 I420 num_layers", desc.num_layers, 3) ||
      expect_swapped_uv_layers("DRM_PRIME_2 I420", &desc)) {
    return 1;
  }

  return 0;
}

static int test_non_drm_prime_is_untouched(
    ExportSurfaceHandleFn export_surface,
    VADriverContextP ctx) {
  VADRMPRIMESurfaceDescriptor desc;
  memset(&desc, 0xa5, sizeof(desc));

  VAStatus status = export_surface(ctx,
                                   VVC_FAKE_SURFACE_I420_THREE_LAYERS,
                                   VVC_FAKE_NON_DRM_MEM_TYPE,
                                   0,
                                   &desc);
  if (expect_status("non-DRM_PRIME_2 export", status, VA_STATUS_SUCCESS) ||
      expect_u32("non-DRM_PRIME_2 fourcc", desc.fourcc, VA_FOURCC_I420) ||
      expect_original_uv_layers("non-DRM_PRIME_2", &desc)) {
    return 1;
  }

  return 0;
}

static int test_failed_export_is_untouched(
    ExportSurfaceHandleFn export_surface,
    VADriverContextP ctx) {
  VADRMPRIMESurfaceDescriptor desc;
  memset(&desc, 0xa5, sizeof(desc));

  VAStatus status = export_surface(ctx,
                                   VVC_FAKE_SURFACE_FAILING_I420_THREE_LAYERS,
                                   VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                   0,
                                   &desc);
  if (expect_status("failed real-driver export",
                    status,
                    VVC_FAKE_FAILURE_STATUS) ||
      expect_u32("failed export fourcc", desc.fourcc, VA_FOURCC_I420) ||
      expect_original_uv_layers("failed export", &desc)) {
    return 1;
  }

  return 0;
}

static int test_short_i420_is_untouched(
    ExportSurfaceHandleFn export_surface,
    VADriverContextP ctx) {
  VADRMPRIMESurfaceDescriptor desc;
  memset(&desc, 0xa5, sizeof(desc));

  VAStatus status = export_surface(ctx,
                                   VVC_FAKE_SURFACE_I420_TWO_LAYERS,
                                   VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                   0,
                                   &desc);
  if (expect_status("two-layer I420 export", status, VA_STATUS_SUCCESS) ||
      expect_u32("two-layer I420 fourcc", desc.fourcc, VA_FOURCC_I420) ||
      expect_u32("two-layer I420 num_layers", desc.num_layers, 2) ||
      expect_original_uv_layers("two-layer I420", &desc)) {
    return 1;
  }

  return 0;
}

static int test_null_descriptor_does_not_crash(
    ExportSurfaceHandleFn export_surface,
    VADriverContextP ctx) {
  VAStatus status = export_surface(ctx,
                                   VVC_FAKE_SURFACE_I420_THREE_LAYERS,
                                   VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                   0,
                                   NULL);
  return expect_status("NULL descriptor export", status, VA_STATUS_SUCCESS);
}

static int test_failed_later_init_preserves_existing_hook(
    InitFn init,
    ExportSurfaceHandleFn export_surface,
    VADriverContextP working_ctx,
    FakeCounterFn init_calls) {
  struct VADriverContext failing_ctx;
  struct VADriverVTable failing_vtable;
  memset(&failing_ctx, 0, sizeof(failing_ctx));
  memset(&failing_vtable, 0, sizeof(failing_vtable));
  failing_ctx.vtable = &failing_vtable;

  if (setenv(VVC_FAKE_FAIL_INIT_ENV, "1", 1) != 0) {
    return fail("setenv failed");
  }
  VAStatus status = init(&failing_ctx);
  unsetenv(VVC_FAKE_FAIL_INIT_ENV);

  if (expect_status("intentional later init failure",
                    status,
                    VVC_FAKE_FAILURE_STATUS) ||
      expect_counter("fake init calls after failed later init", init_calls(), 2)) {
    return 1;
  }

  VADRMPRIMESurfaceDescriptor desc;
  memset(&desc, 0xa5, sizeof(desc));
  status = export_surface(working_ctx,
                          VVC_FAKE_SURFACE_I420_THREE_LAYERS,
                          VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                          0,
                          &desc);
  if (expect_status("existing hook after failed later init",
                    status,
                    VA_STATUS_SUCCESS) ||
      expect_u32("existing hook after failed later init fourcc",
                 desc.fourcc,
                 VA_FOURCC_YV12)) {
    return 1;
  }

  return 0;
}

int main(int argc, char **argv) {
  if (argc != 4) {
    return fail("usage: harness <shim.so> <fake-real-driver.so> <init-symbol>");
  }

  if (strcmp(argv[3], VVC_DERIVED_INIT_SYMBOL) != 0) {
    fprintf(stderr,
            "init symbol %s does not match libva ABI %s\n",
            argv[3],
            VVC_DERIVED_INIT_SYMBOL);
    return 1;
  }

  if (VVC_FAKE_NON_DRM_MEM_TYPE == VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2) {
    return fail("test non-DRM mem type unexpectedly equals DRM_PRIME_2");
  }

  void *fake = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
  if (!fake) {
    return fail(dlerror());
  }

  FakeResetFn fake_reset =
      (FakeResetFn)load_symbol(fake, "virgl_vaapi_compat_test_fake_reset");
  FakeCounterFn fake_init_calls =
      (FakeCounterFn)load_symbol(fake,
                                 "virgl_vaapi_compat_test_fake_init_calls");
  FakeCounterFn fake_export_calls =
      (FakeCounterFn)load_symbol(fake,
                                 "virgl_vaapi_compat_test_fake_export_calls");
  FakeExportFunctionFn fake_export_function =
      (FakeExportFunctionFn)load_symbol(
          fake,
          "virgl_vaapi_compat_test_fake_export_function");
  if (!fake_reset || !fake_init_calls || !fake_export_calls ||
      !fake_export_function) {
    return 1;
  }

  fake_reset();

  void *shim = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!shim) {
    return fail(dlerror());
  }

  InitFn init = (InitFn)load_symbol(shim, argv[3]);
  if (!init) {
    return 1;
  }

  struct VADriverContext ctx;
  struct VADriverVTable vtable;
  memset(&ctx, 0, sizeof(ctx));
  memset(&vtable, 0, sizeof(vtable));
  ctx.vtable = &vtable;

  if (expect_status("shim init", init(&ctx), VA_STATUS_SUCCESS) ||
      expect_counter("fake init calls", fake_init_calls(), 1)) {
    return 1;
  }

  if (!ctx.vtable->vaExportSurfaceHandle) {
    return fail("shim did not install vaExportSurfaceHandle");
  }
  if (ctx.vtable->vaExportSurfaceHandle == fake_export_function()) {
    return fail("shim left the fake real-driver export hook installed");
  }

  ExportSurfaceHandleFn export_surface = ctx.vtable->vaExportSurfaceHandle;
  if (test_drm_prime_i420_rewrite(export_surface, &ctx, fake_export_calls) ||
      test_non_drm_prime_is_untouched(export_surface, &ctx) ||
      test_failed_export_is_untouched(export_surface, &ctx) ||
      test_short_i420_is_untouched(export_surface, &ctx) ||
      test_null_descriptor_does_not_crash(export_surface, &ctx) ||
      test_failed_later_init_preserves_existing_hook(
          init,
          export_surface,
          &ctx,
          fake_init_calls)) {
    return 1;
  }

  return 0;
}
