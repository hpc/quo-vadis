#
# Copyright (c) 2020-2023 Triad National Security, LLC
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
      qvi-common.h
      qvi-macros.h
      qvi-log.h
      qvi-utils.h
      qvi-bbuff.h
      qvi-hwloc.h
      qvi-nvml.h
      qvi-rsmi.h
      qvi-line.h
      qvi-hwpool.h
      qvi-rmi.h
      qvi-bbuff-rmi.h
      qvi-context.h
      qvi-task.h
      qvi-devinfo.h
      qvi-group.h
      qvi-map.h
      qvi-scope.h
      qvi-zgroup.h
      qvi-bind.h
      qvi-log.cc
      qvi-utils.cc
      qvi-bbuff.cc
      qvi-hwloc.cc
      qvi-nvml.cc
      qvi-rsmi.cc
      qvi-line.cc
      qvi-hwpool.cc
      qvi-rmi.cc
      qvi-context.cc
      qvi-task.cc
      qvi-group.cc
      qvi-map.cc
      qvi-scope.cc
      qvi-bind.cc
      quo-vadis.cc
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
    qvi-zgroup-process.h
    qvi-process.cc
    qvi-group-process.cc
    qvi-zgroup-process.cc
    quo-vadis-process.cc
)

# TODO(skg) We need to fix this. OpenMP builds versus Pthread.
if(OpenMP_C_FOUND)
set(
    QUO_VADIS_SOURCE
    ${QUO_VADIS_SOURCE}
    qvi-thread.h
    qvi-zgroup-thread.h
    qvi-group-thread.h
    ../include/quo-vadis-thread.h
    qvi-thread.cc
    qvi-zgroup-thread.cc
    qvi-group-thread.cc
    quo-vadis-thread.cc
)
endif()

add_library(
    quo-vadis
    SHARED
      ${QUO_VADIS_SOURCE}
)

if(OpenMP_C_FOUND)
    set_target_properties(
        quo-vadis
        PROPERTIES
          COMPILE_FLAGS ${OpenMP_C_FLAGS}
          LINK_FLAGS ${OpenMP_C_FLAGS}
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
          qvi-zgroup-mpi.h
          qvi-mpi.cc
          qvi-group-mpi.cc
          qvi-zgroup-mpi.cc
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
