# -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*-
#
# Copyright (c) 2020-2024 Triad National Security, LLC
#                         All rights reserved.
#
# Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

################################################################################
################################################################################
add_library(
    quo-vadis-core
    OBJECT
      ../include/quo-vadis.h
      ../include/quo-vadis-pthread.h
      qvi-common.h
      qvi-macros.h
      qvi-log.h
      qvi-utils.h
      qvi-bbuff.h
      qvi-hwloc.h
      qvi-nvml.h
      qvi-rsmi.h
      qvi-hwpool.h
      qvi-rmi.h
      qvi-bbuff-rmi.h
      qvi-task.h
      qvi-pthread.h
      qvi-group.h
      qvi-subgroup.h
      qvi-group-pthread.h
      qvi-map.h
      qvi-hwsplit.h
      qvi-scope.h
      qvi-log.cc
      qvi-utils.cc
      qvi-bbuff.cc
      qvi-hwloc.cc
      qvi-nvml.cc
      qvi-rsmi.cc
      qvi-hwpool.cc
      qvi-rmi.cc
      qvi-task.cc
      qvi-group.cc
      qvi-pthread.cc
      qvi-group-pthread.cc
      qvi-map.cc
      qvi-hwsplit.cc
      qvi-scope.cc
      quo-vadis.cc
      quo-vadis-pthread.cc
)

set_target_properties(
    quo-vadis-core
    PROPERTIES
      INTERFACE_LINK_LIBRARIES "spdlog;hwloc;zmq"
      POSITION_INDEPENDENT_CODE ON
)

# For parallel build correctness explicitly add dependencies.
add_dependencies(
    quo-vadis-core
    spdlog
    hwloc
)

target_compile_options(
    quo-vadis-core
    PUBLIC
      -DSPDLOG_COMPILED_LIB
)

target_include_directories(
    quo-vadis-core
    PUBLIC
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
      $<TARGET_PROPERTY:spdlog,INTERFACE_INCLUDE_DIRECTORIES>
      $<TARGET_PROPERTY:hwloc,INTERFACE_INCLUDE_DIRECTORIES>
      $<TARGET_PROPERTY:zmq,INTERFACE_INCLUDE_DIRECTORIES>
)

################################################################################
# Base library
################################################################################
set(
    QUO_VADIS_SOURCE
    ../include/quo-vadis-process.h
    qvi-process.h
    qvi-group-process.h
    qvi-process.cc
    qvi-group-process.cc
    quo-vadis-process.cc
)

add_library(
    quo-vadis
    SHARED
      ${QUO_VADIS_SOURCE}
)

if(OpenMP_C_FOUND)
    add_library(
        quo-vadis-omp
        SHARED
          qvi-omp.h
          qvi-group-omp.h
          ../include/quo-vadis-omp.h
          qvi-omp.cc
          qvi-group-omp.cc
          quo-vadis-omp.cc
    )

    set_target_properties(
        quo-vadis-omp
        PROPERTIES
          COMPILE_FLAGS ${OpenMP_C_FLAGS}
          LINK_FLAGS ${OpenMP_C_FLAGS}
    )

    target_include_directories(
        quo-vadis-omp
        PUBLIC
          $<TARGET_PROPERTY:quo-vadis-core,INCLUDE_DIRECTORIES>
          ${MPI_C_INCLUDE_DIRS}
    )

    target_link_libraries(
        quo-vadis-omp
        quo-vadis-core
        ${MPI_C_LIBRARIES}
    )

    install(
        TARGETS
          quo-vadis-omp
        DESTINATION
          lib
    )

    # pkg-config ###############################################################
    set(QV_PKG_LIBRARIES "-lquo-vadis-omp")
    configure_file(
        "${CMAKE_SOURCE_DIR}/pkgconfig/pkg-config.pc.in"
        "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-omp.pc"
        @ONLY
    )

    install(
        FILES
          "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-omp.pc"
        DESTINATION
          lib/pkgconfig
    )
endif()

target_include_directories(
    quo-vadis
    PUBLIC
      $<TARGET_PROPERTY:quo-vadis-core,INCLUDE_DIRECTORIES>
)

target_link_libraries(
    quo-vadis
    quo-vadis-core
)

################################################################################
################################################################################
if(MPI_FOUND)
    add_library(
        quo-vadis-mpi
        SHARED
          ../include/quo-vadis-mpi.h
          qvi-mpi.h
          qvi-group-mpi.h
          qvi-mpi.cc
          qvi-group-mpi.cc
          quo-vadis-mpi.cc
    )

    target_include_directories(
        quo-vadis-mpi
        PUBLIC
          $<TARGET_PROPERTY:quo-vadis-core,INCLUDE_DIRECTORIES>
          ${MPI_C_INCLUDE_DIRS}
    )

    target_link_libraries(
        quo-vadis-mpi
        quo-vadis-core
        ${MPI_C_LIBRARIES}
    )

    install(
        TARGETS
          quo-vadis-mpi
        DESTINATION
          lib
    )

    # pkg-config ###############################################################
    set(QV_PKG_LIBRARIES "-lquo-vadis-mpi")
    configure_file(
        "${CMAKE_SOURCE_DIR}/pkgconfig/pkg-config.pc.in"
        "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-mpi.pc"
        @ONLY
    )

    install(
        FILES
          "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-mpi.pc"
        DESTINATION
          lib/pkgconfig
    )
endif()

install(
    TARGETS
      quo-vadis
    DESTINATION
      lib
)

# pkg-config ###############################################################
set(QV_PKG_LIBRARIES "-lquo-vadis")
configure_file(
    "${CMAKE_SOURCE_DIR}/pkgconfig/pkg-config.pc.in"
    "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc"
    @ONLY
)

install(
    FILES
      "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc"
    DESTINATION
      lib/pkgconfig
)

################################################################################
################################################################################
add_executable(
    quo-vadisd
    quo-vadisd.cc
)

target_link_libraries(
    quo-vadisd
    quo-vadis-core
)

install(
    TARGETS
      quo-vadisd
    DESTINATION
      bin
)

# vim: ts=4 sts=4 sw=4 expandtab
