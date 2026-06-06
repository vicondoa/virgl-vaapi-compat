# AGENTS.md

Operating guide for AI coding agents and humans working on
`vicondoa/virgl-vaapi-compat`.

## Repository purpose

`virgl-vaapi-compat` is a narrow libva driver-wrapper project. It builds a
replacement `virtio_gpu_drv_video.so` that delegates to the real virtio-gpu VA
driver and only adjusts selected `vaExportSurfaceHandle` DRM PRIME descriptors
for virgl/virtio-gpu compatibility.

The project is licensed under Apache-2.0. Keep license references consistent
with `LICENSE`.

## Layout

```text
.
├── AGENTS.md
├── CHANGELOG.md
├── CONTRIBUTING.md
├── LICENSE
├── Makefile
├── README.md
├── SECURITY.md
├── src/
│   └── virtio_gpu_drv_video.c
└── tests/
    ├── fake-virtio-gpu-driver.c
    └── harness.c
```

- `src/` contains the production C shim.
- `tests/` contains the fake driver and executable test harness.
- `Makefile` is the source of truth for build, test, install, and clean
  commands.
- Governance documents live at the repository root.

## Build and test

Requirements:

- C compiler
- `pkg-config`
- libva headers
- `make`

Commands:

```bash
make
./tests/static.sh
make clean
```

Useful overrides:

```bash
make CC=clang
make REAL_DRIVER=/path/to/virtio_gpu_drv_video.so
make install PREFIX=/usr/local
```

Run `./tests/static.sh` before submitting non-documentation changes. For fast C
iterations, `make test` is acceptable while developing, but the static gate is
the pre-review and CI parity check.

## Coding conventions

- Use C11 with the existing `-Wall -Wextra -Werror -fPIC` build flags.
- Keep the shim narrow: hook only the VA-API behavior required for
  compatibility, and pass everything else through unchanged.
- Prefer small functions with explicit error handling over broad rewrites.
- Do not add runtime dependencies unless they are essential for the wrapper.
- Preserve the existing two-space indentation style for C and Markdown. Use
  tabs only where Makefile syntax requires them.
- Add comments only for behavior that would surprise a future maintainer.

## Consumer configuration boundaries

Do not edit consumer machine configuration, including `/etc/nixos`, Home
Manager files, NixOS flakes, VM definitions, browser wrappers, or downstream
deployment glue, unless the maintainer explicitly approves that scope.

This repository may document integration points, but it must not silently make
host-specific changes for a consumer.

## Nixling-style review panel policy

Non-trivial plan-driven or multi-phase work requires unanimous 8/8 panel
approval at each phase boundary before moving to the next phase. The required
boundaries are:

1. Plan review
2. Implementation review
3. Integration review
4. Work review

The panel has eight reviewers:

1. `software`
2. `systems`
3. `security`
4. `compatibility`
5. `testing`
6. `documentation`
7. `release`
8. `maintainer`

Each reviewer must return JSON in this shape:

```json
{
  "engineer": "software",
  "signoff": true,
  "summary": "No blocking issues found.",
  "recommendations": []
}
```

Rules:

- `signoff: true` is valid if and only if `recommendations` is empty.
- Any recommendation is a blocker for that phase.
- No next phase may begin until all eight reviewers return `signoff: true`.
- Green builds, tests, or local smoke checks do not waive unanimous sign-off.
- Trivial typo-only changes may skip the panel.
- Load-bearing documentation, governance policy, security posture, release
  notes, and integration instructions are not trivial and must use the panel
  for plan-driven/multi-phase work.

If there is uncertainty about whether a change is trivial, use the panel.

## Branch protection expectation

`main` is intended to be protected once the bootstrap branch lands. Require PRs,
fresh approval after new commits, passing CI jobs, and maintainer bypass only for
emergency repository repair. Non-trivial plan-driven work still requires the 8/8
panel even if GitHub's branch rule only enforces ordinary PR approval.
