#!/usr/bin/env sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo_root"

log() {
  printf '\n==> %s\n' "$*"
}

run() {
  log "$*"
  "$@"
}

check_shell_syntax() {
  log "checking shell script syntax"
  find . \
    -path ./.git -prune -o \
    -path ./build -prune -o \
    -path ./result -prune -o \
    -path './result-*' -prune -o \
    -type f -name '*.sh' -print | sort | \
    while IFS= read -r script; do
      first_line=$(sed -n '1p' "$script")
      case "$first_line" in
        *bash*) checker=bash ;;
        *) checker=sh ;;
      esac
      printf '    %s -n %s\n' "$checker" "$script"
      "$checker" -n "$script"
    done
}

run_flake_checks_if_present() {
  if [ ! -f flake.nix ]; then
    log "skipping Nix flake checks; flake.nix is not present"
    return 0
  fi

  if ! command -v nix >/dev/null 2>&1; then
    echo "flake.nix exists, but nix is not available in PATH" >&2
    exit 1
  fi

  run nix flake check --no-build --all-systems
  run nix build '.#checks.x86_64-linux.default'
}

# Keep the local gate non-privileged: the project tests compile and exercise a
# fake userspace VA-API driver, so no sudo, device access, or self-hosted runner
# privileges are required.
check_shell_syntax
run make test
run_flake_checks_if_present
