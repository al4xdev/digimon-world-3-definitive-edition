#!/usr/bin/env bash
# apt_install.sh PACKAGE...: apt-get update + install on a CI runner, bounded and retried.
#
# The runners' Ubuntu mirror stalls on a fetch now and then: a 4.5 min `apt-get install gcc-multilib` (2026-10-07),
# and the v0.7.4 release's Windows job hung twice in "System packages" for 13+ min while other runs of the same hour
# took 20 s. A stall is the machine, not the packages, so each attempt gets a deadline (DW3_APT_DEADLINE seconds,
# default 300) and up to three attempts; apt itself retries a failed fetch and gives up on a silent connection after
# 30 s. man-db's trigger (rebuilding the manual's index after an install, minutes with Wine) is switched off: CI
# reads no manual pages.
#
# Extra apt-get install options go before the packages, e.g. --no-install-recommends.
set -euo pipefail
[ "$#" -gt 0 ] || { echo "usage: $0 [apt-get install options] PACKAGE..." >&2; exit 2; }

deadline="${DW3_APT_DEADLINE:-300}"
opts=(-q -o Acquire::Retries=3 -o Acquire::http::Timeout=30 -o Acquire::https::Timeout=30 -o DPkg::Lock::Timeout=120)

echo "man-db man-db/auto-update boolean false" | sudo debconf-set-selections
sudo rm -f /var/lib/man-db/auto-update

for attempt in 1 2 3; do
    if sudo timeout "$deadline" apt-get "${opts[@]}" update &&
        sudo DEBIAN_FRONTEND=noninteractive timeout "$deadline" apt-get "${opts[@]}" install -y "$@"; then
        exit 0
    fi
    echo "apt_install: attempt $attempt failed or passed ${deadline}s" >&2
    # an install cut off by the deadline leaves dpkg half done; finish what it unpacked before trying again
    sudo DEBIAN_FRONTEND=noninteractive dpkg --configure -a || true
    sleep $((attempt * 10))
done
echo "apt_install: gave up after 3 attempts" >&2
exit 1
