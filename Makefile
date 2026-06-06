PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib/dri
DRIVER_NAME ?= virtio_gpu_drv_video.so
REAL_DRIVER ?= /run/opengl-driver/lib/dri/virtio_gpu_drv_video.so
BUILD_DIR ?= build

PKG_CONFIG ?= pkg-config
CC ?= cc
INSTALL ?= install

LIBVA_CFLAGS := $(shell $(PKG_CONFIG) --cflags libva 2>/dev/null)
ALL_CPPFLAGS = $(CPPFLAGS) $(LIBVA_CFLAGS)

libva_macro = $(strip $(shell printf '%s\n%s\n' '#include <va/va_version.h>' '$(1)' | $(CC) $(ALL_CPPFLAGS) -E -P -x c - 2>/dev/null | tail -n 1))

LIBVA_INIT_ABI_MAJOR ?= $(call libva_macro,VA_MAJOR_VERSION)
LIBVA_INIT_ABI_MINOR ?= $(call libva_macro,VA_MINOR_VERSION)
INIT_SYMBOL := __vaDriverInit_$(LIBVA_INIT_ABI_MAJOR)_$(LIBVA_INIT_ABI_MINOR)

CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Werror -fPIC
LDLIBS += -ldl

ABI_CPPFLAGS = \
	-DVIRGL_VAAPI_COMPAT_ABI_MAJOR=$(LIBVA_INIT_ABI_MAJOR) \
	-DVIRGL_VAAPI_COMPAT_ABI_MINOR=$(LIBVA_INIT_ABI_MINOR)

REAL_DRIVER_CPPFLAGS = -DVIRGL_VAAPI_COMPAT_REAL_DRIVER=\"$(REAL_DRIVER)\"

SHIM := $(BUILD_DIR)/$(DRIVER_NAME)
TEST_REAL_DRIVER := $(BUILD_DIR)/real/$(DRIVER_NAME)
TEST_SHIM := $(BUILD_DIR)/test/$(DRIVER_NAME)
HARNESS := $(BUILD_DIR)/harness
LIBVA_PROBE := $(BUILD_DIR)/core-libva-probe.o
BUILD_CONFIG := $(BUILD_DIR)/config.mk

.DELETE_ON_ERROR:

.PHONY: all check-libva check-real-driver clean install test FORCE

all: $(SHIM)

check-libva:
	@command -v $(PKG_CONFIG) >/dev/null 2>&1 || { echo "pkg-config not found; install pkg-config and libva development headers" >&2; exit 1; }
	@$(PKG_CONFIG) --exists libva || { echo "pkg-config could not find libva; install libva development headers or set PKG_CONFIG_PATH" >&2; exit 1; }
	@case "$(LIBVA_INIT_ABI_MAJOR)" in ''|*[!0-9]*) echo "could not derive VA_MAJOR_VERSION from libva headers" >&2; exit 1;; esac
	@case "$(LIBVA_INIT_ABI_MINOR)" in ''|*[!0-9]*) echo "could not derive VA_MINOR_VERSION from libva headers" >&2; exit 1;; esac

check-real-driver:
	@case "$(REAL_DRIVER)" in /*) ;; *) echo "REAL_DRIVER must be an absolute path; got '$(REAL_DRIVER)'" >&2; exit 1;; esac

$(BUILD_DIR) $(BUILD_DIR)/real $(BUILD_DIR)/test:
	mkdir -p $@

$(BUILD_CONFIG): FORCE | check-libva $(BUILD_DIR)
	@tmp="$@.tmp"; \
	{ \
		printf '%s\n' 'CC=$(CC)'; \
		printf '%s\n' 'CPPFLAGS=$(CPPFLAGS)'; \
		printf '%s\n' 'CFLAGS=$(CFLAGS)'; \
		printf '%s\n' 'LDFLAGS=$(LDFLAGS)'; \
		printf '%s\n' 'LDLIBS=$(LDLIBS)'; \
		printf '%s\n' 'REAL_DRIVER=$(REAL_DRIVER)'; \
		printf '%s\n' 'DRIVER_NAME=$(DRIVER_NAME)'; \
		printf '%s\n' 'LIBVA_INIT_ABI_MAJOR=$(LIBVA_INIT_ABI_MAJOR)'; \
		printf '%s\n' 'LIBVA_INIT_ABI_MINOR=$(LIBVA_INIT_ABI_MINOR)'; \
	} > "$$tmp"; \
	if test -r "$@" && cmp -s "$$tmp" "$@"; then \
		rm -f "$$tmp"; \
	else \
		mv "$$tmp" "$@"; \
	fi

$(SHIM): src/virtio_gpu_drv_video.c $(BUILD_CONFIG) | check-real-driver $(BUILD_DIR)
	$(CC) $(ALL_CPPFLAGS) $(ABI_CPPFLAGS) $(REAL_DRIVER_CPPFLAGS) $(CFLAGS) \
		$(LDFLAGS) -shared -Wl,-soname,$(DRIVER_NAME) -o $@ $< $(LDLIBS)

$(TEST_REAL_DRIVER): tests/fake-virtio-gpu-driver.c tests/core-fake-driver.h $(BUILD_CONFIG) | $(BUILD_DIR)/real
	$(CC) $(ALL_CPPFLAGS) $(ABI_CPPFLAGS) $(CFLAGS) \
		$(LDFLAGS) -shared -Wl,-soname,$(DRIVER_NAME) -o $@ $<

$(TEST_SHIM): src/virtio_gpu_drv_video.c $(TEST_REAL_DRIVER) $(BUILD_CONFIG) | $(BUILD_DIR)/test
	$(CC) $(ALL_CPPFLAGS) $(ABI_CPPFLAGS) \
		-DVIRGL_VAAPI_COMPAT_REAL_DRIVER=\"$(abspath $(TEST_REAL_DRIVER))\" \
		$(CFLAGS) $(LDFLAGS) -shared -Wl,-soname,$(DRIVER_NAME) -o $@ $< $(LDLIBS)

$(HARNESS): tests/harness.c tests/core-fake-driver.h $(BUILD_CONFIG) | $(BUILD_DIR)
	$(CC) $(ALL_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

$(LIBVA_PROBE): tests/core-libva-probe.c $(BUILD_CONFIG) | $(BUILD_DIR)
	$(CC) $(ALL_CPPFLAGS) $(CFLAGS) -c -o $@ $<

test: $(LIBVA_PROBE) $(TEST_SHIM) $(HARNESS)
	$(HARNESS) $(abspath $(TEST_SHIM)) $(abspath $(TEST_REAL_DRIVER)) $(INIT_SYMBOL)

install: $(SHIM)
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 0755 $(SHIM) $(DESTDIR)$(LIBDIR)/$(DRIVER_NAME)

clean:
	rm -rf $(BUILD_DIR)
