#!/bin/bash
#
# Copyright (c) 2023-2026 Triad National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

# Ensure a stray QV_PORT from the caller's is not set.
unset QV_PORT

# Callers must export QV_DAEMON to the quo-vadisd binary's path (e.g. via the
# CMake $<TARGET_FILE:quo-vadisd> generator expression) so the daemon can be
# found regardless of the test's working directory.
if [ -z "${QV_DAEMON}" ]; then
    echo "$0: QV_DAEMON is not set; it must point to the quo-vadisd binary" >&2
    exit 1
fi
"$QV_DAEMON" --no-daemonize --port 55999 &
qvdpid=$!

exit_code=0

to_exec="${*:1}"
echo "Starting $to_exec"
if ! $to_exec
then
    exit_code=1
fi

kill $qvdpid
exit $exit_code
