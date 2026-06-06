# Security Policy

`virgl-vaapi-compat` is a small libva driver wrapper. It is not setuid, does not
intend to provide sandboxing, and runs with the privileges of the VA-API client
process that loads it.

The project is licensed under Apache-2.0.

## Reporting a vulnerability

Please report suspected vulnerabilities through GitHub Security Advisories for
this repository. Do not open a public issue for embargoed or exploitable
security reports.

Include:

- Affected commit, tag, or version.
- Reproduction steps or proof of concept.
- Impact analysis.
- Relevant platform, libva, Mesa, virgl/virtio-gpu, and client versions.
- Whether the issue requires a malicious client, malicious driver path, hostile
  media input, or only normal use.

Maintainers will review reports on a best-effort basis, coordinate fixes in
private when appropriate, and publish advisories or release notes once a fix is
available.

## Supported versions

This repository is pre-stable. Until the first stable release, security fixes
target the default branch and the latest alpha release line when practical.

## Threat model scope

In scope:

- Memory-safety bugs in the wrapper.
- Incorrect validation or mutation of `VADRMPRIMESurfaceDescriptor` data.
- Unsafe dynamic loading behavior introduced by this shim.
- Bugs that cause the wrapper to corrupt client process memory or redirect to
  an unintended real driver.
- Debug logging that exposes sensitive process or driver-path information.

Out of scope:

- Vulnerabilities in upstream libva, Mesa, virglrenderer, rutabaga, crosvm,
  browsers, kernels, GPU firmware, or distribution packages.
- Bugs requiring local write access to replace the wrapper or the real VA
  driver on disk.
- General VA-API client sandbox escapes not caused by this shim.
- Downstream packaging, NixOS, Home Manager, browser wrapper, or VM integration
  configuration unless that code is added to this repository.

## Security design notes

- The wrapper should be as narrow as possible and pass through all behavior it
  does not explicitly need to adjust.
- The shim must not silently edit host or consumer configuration.
- Avoid adding environment-variable controls that widen the attack surface.
- Prefer fail-closed behavior when the real driver cannot be loaded or required
  symbols cannot be resolved.
