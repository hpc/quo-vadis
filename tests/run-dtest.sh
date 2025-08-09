#!/bin/bash
#
# Copyright (c) 2023-2025 Triad National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

startd="../src/quo-vadisd --no-daemonize --port 55999"
$startd &
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
