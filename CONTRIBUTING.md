# Contributing

Thanks for helping improve `virgl-vaapi-compat`.

This project is licensed under Apache-2.0. Unless you clearly state otherwise,
contributions submitted to this repository are accepted under that license.

## Issues

Open an issue before large or behavior-changing work. Include:

- Host and guest environment details.
- libva, Mesa, virgl/virtio-gpu, and client versions when relevant.
- The expected behavior and the observed behavior.
- Reproduction steps, logs, and whether `VIRGL_VAAPI_COMPAT_DEBUG=1` changes
  the diagnosis.

Use GitHub Security Advisories instead of public issues for security reports;
see `SECURITY.md`.

## Local setup

Install:

- C compiler
- `pkg-config`
- libva headers
- `make`

Build and test:

```bash
make
./tests/static.sh
```

Optional overrides:

```bash
make CC=clang
make REAL_DRIVER=/path/to/virtio_gpu_drv_video.so
make install PREFIX=/usr/local
```

## Quality gates

Before opening a pull request:

- Keep the diff focused on one problem.
- Run `./tests/static.sh` for code, build, packaging, or CI changes. This is the
  local gate mirrored by CI; it runs shell syntax checks, `make test`, and flake
  checks when `flake.nix` is present.
- Run `make test` at minimum for quick non-Nix C iterations before the full gate.
- For documentation-only changes, review the rendered Markdown and links.
- Confirm no secrets, host-specific paths, or consumer machine edits are
  included.
- Do not modify `/etc/nixos` or other downstream consumer configuration unless
  the maintainer explicitly approved that scope.
- Update `CHANGELOG.md` for user-visible changes.
- Update `README.md`, `SECURITY.md`, or this guide when behavior, support, or
  process changes.

If a required local tool is unavailable, state that clearly in the pull request
with the command attempted and the failure.

## Pull requests

After initial repository bootstrap, nobody direct-pushes to `main`. GitHub's
required approval count is intentionally 0 so the solo maintainer can merge
their own PR after CI and any required panel sign-off. Start from a topic
branch, run the relevant local validation, open a pull request, wait for
required CI and conversation resolution, and squash-merge only after required
checks and panel requirements are satisfied.

Pull requests should include:

- A concise summary.
- Relevant issue links.
- Testing performed.
- Any compatibility or security considerations.
- Panel review status when the change is non-trivial and plan-driven.

## Commit messages

Use short imperative subjects:

```text
shim: preserve non-DRM PRIME exports
tests: cover I420 passthrough
docs: explain driver path override
```

Keep unrelated changes in separate commits. Avoid vague subjects such as
`update files` or `fix stuff`.

## Review panel expectations

Non-trivial plan-driven or multi-phase work follows the 8-reviewer panel
sign-off policy.

At each phase boundary, work must receive 8/8 sign-off before the next phase
starts:

1. Plan review
2. Implementation review
3. Integration review
4. Work review

The reviewers are `software`, `systems`, `security`, `compatibility`,
`testing`, `documentation`, `release`, and `maintainer`.

Each reviewer returns JSON:

```json
{
  "engineer": "software",
  "signoff": true,
  "summary": "No blocking issues found.",
  "recommendations": []
}
```

`signoff: true` is valid only when `recommendations` is empty. Green tests do
not replace unanimous sign-off. Trivial typo-only changes may skip the panel,
but load-bearing documentation, governance policy, security posture, release
notes, and integration instructions may not.

## Branch protection

The public `main` branch should be protected after the initial bootstrap push:

- block direct pushes for everyone;
- require pull requests before merge;
- keep GitHub-native required approval count at 0 for solo-maintainer
  self-merge;
- require the documented 8/8 panel for non-trivial plan-driven work;
- dismiss stale approvals when new commits are pushed;
- require the `make test (Ubuntu packages)` and `optional Nix flake checks`
  workflow jobs to pass;
- require all conversations to be resolved before merge;
- disallow force-pushes;
- disallow branch deletion;
- restrict maintainer bypass to emergency repository repair and document any
  bypass in the PR or follow-up issue.
