#!/usr/bin/env bash
set -Eeuo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
ramulator_root="${workspace_root}/thirdparty/ramulator2"

if [[ ! -f "${ramulator_root}/requirements-dev.txt" ]]; then
  echo "Ramulator 2.1 was not found at ${ramulator_root}" >&2
  exit 1
fi

# Install the runnable package first.  The development-only requirements
# include large analysis packages and must not delay container readiness.
python -m pip install --editable "${ramulator_root}"

if [[ "${OPENHBF_INSTALL_RAMULATOR_DEV_DEPS:-0}" == "1" ]]; then
  python -m pip install -r "${ramulator_root}/requirements-dev.txt"
fi
