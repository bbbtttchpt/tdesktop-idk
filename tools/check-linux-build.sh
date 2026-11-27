#!/usr/bin/env bash
set -Eeuo pipefail

IMAGE="tdesktop:centos_env"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

errors=0
docker_ready=0

ok() {
  printf '[OK] %s\n' "$1"
}

warn() {
  printf '[WARN] %s\n' "$1"
}

fail() {
  printf '[ERROR] %s\n' "$1"
  errors=$((errors + 1))
}

printf 'Telegram Desktop Linux build preflight\n'
printf 'Repository: %s\n\n' "$REPO_ROOT"

if [[ "$(uname -s)" != "Linux" ]]; then
  fail "This checker targets the Docker-based Linux build. Run it from Linux or WSL."
fi

if ! git -C "$REPO_ROOT" rev-parse --show-toplevel >/dev/null 2>&1; then
  fail "The source directory is not a Git checkout."
else
  ok "Git checkout found."
fi

if command -v git >/dev/null 2>&1; then
  if submodule_status="$(git -C "$REPO_ROOT" submodule status --recursive 2>&1)"; then
    incomplete=0
    while IFS= read -r line; do
      case "$line" in
        -*|+*)
          printf '[ERROR] Submodule is missing or checked out at the wrong commit: %s\n' "$line"
          incomplete=1
          ;;
      esac
    done <<< "$submodule_status"
    if [[ "$incomplete" -eq 0 ]]; then
      ok "Git submodules are initialized at the recorded commits."
    else
      errors=$((errors + 1))
      printf '       Fix with: git submodule update --init --recursive\n'
    fi
  else
    fail "Could not inspect Git submodules: $submodule_status"
  fi
else
  fail "git is not installed or is not on PATH."
fi

if command -v docker >/dev/null 2>&1; then
  if docker info >/dev/null 2>&1; then
    docker_ready=1
    ok "Docker CLI and daemon are available."
  else
    fail "Docker is installed, but the daemon is unavailable. Start Docker and check your permissions."
  fi
else
  fail "Docker is not installed or is not on PATH."
fi

if [[ "$docker_ready" -eq 1 ]]; then
  if docker image inspect "$IMAGE" >/dev/null 2>&1; then
    ok "Build image $IMAGE exists."
    if docker run --rm --entrypoint /bin/bash "$IMAGE" -lc '
      set -e
      command -v cmake >/dev/null
      command -v ninja >/dev/null
      command -v gcc >/dev/null
      command -v g++ >/dev/null
      command -v python3 >/dev/null
      cmake --version >/dev/null
      ninja --version >/dev/null
    '; then
      ok "Build image contains CMake, Ninja, GCC, G++, and Python."
    else
      fail "The build image is present, but its compiler/toolchain check failed."
    fi
  else
    fail "Build image $IMAGE is missing. Run Telegram/build/prepare/linux.sh from the repository root."
    if command -v poetry >/dev/null 2>&1; then
      ok "Poetry is installed and can prepare the Docker image."
    else
      fail "Poetry is missing. Install Poetry before running the Linux preparation script."
    fi
  fi
fi

printf '\n'
if [[ "$errors" -eq 0 ]]; then
  printf 'Preflight passed. You can proceed with the Linux build instructions.\n'
else
  printf 'Preflight found %s problem(s). Fix the errors above and run it again.\n' "$errors"
  exit 1
fi
