/*
 * Copyright (c)      2020 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2020 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-threads.c
 */

#include "qvi-macros.h"
//#include "qvi-thread-mgmt.h"

#include "quo-vadis.h"

#include <stdlib.h>
#include <stdio.h>

int
main(
     int argc,
     char *argv[]
){
  fprintf(stdout,"# Starting test\n");
  
  qv_thread_mgmt_toto();
  
  return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
