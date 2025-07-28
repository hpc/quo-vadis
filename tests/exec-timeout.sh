#!/bin/bash
#
# Copyright (c) 2023-2025 Triad National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

argc="$#"

if [[ $argc -lt 2 ]]; then
    echo "usage: $1 timeout_in_seconds to_exec [ARGS]"
    exit 1
fi

timeout_in_seconds="$1"
to_exec="${*:2}"

echo "Starting $to_exec"
$to_exec &
qvdpid=$!

sleep "$timeout_in_seconds" && kill $qvdpid
