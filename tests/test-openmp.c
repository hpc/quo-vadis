/*
 * Copyright (c)      2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-openmp.c
 *
 * Used to verify OpenMP support.
 */

#include <stdlib.h>
#include <stdio.h>
#include <omp.h>

int
main(
     int argc,
     char *argv[]
){
    printf("# Starting OpenMP test\n");

    printf("# OpenMP Test Max Threads=%d\n ", omp_get_max_threads());

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
