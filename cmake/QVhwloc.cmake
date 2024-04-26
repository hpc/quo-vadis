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

# Includes support for external projects
include(ExternalProject)

set(QVI_HWLOC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/hwloc-2.5.0.tar.gz)
set(QVI_HWLOC_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/hwloc)
set(QVI_HWLOC_STATIC_LIB ${QVI_HWLOC_PREFIX}/lib/libhwloc.a)
set(QVI_HWLOC_INCLUDES ${QVI_HWLOC_PREFIX}/include)
set(QVI_HWLOC_PKG_CONFIG ${QVI_HWLOC_PREFIX}/lib/pkgconfig)
# A list of configure variables.
set(QVI_HWLOC_CONFIG_VARS "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}")
# A list containing any relevant GPU flags required in hwloc configuration.
set(QVI_HWLOC_GPU_FLAGS "")

file(MAKE_DIRECTORY ${QVI_HWLOC_INCLUDES})

if(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
    # Workaround for the following warning when attempting to perform make -j:
    # make[3]: warning: jobserver unavailable:
    # using -j1.  Add '+' to parent make rule.
    set(QVI_HWLOC_BUILD_COMMAND "\$(MAKE)")
else()
    # Ninja doesn't like $(MAKE), so just use make
    set(QVI_HWLOC_BUILD_COMMAND "make")
endif()

set(QVI_HWLOC_EXTRA_CONFIG_FLAGS "--enable-silent-rules=yes")
# If the top-level build is verbose, also enable verbose make output here.
if(CMAKE_VERBOSE_MAKEFILE)
    set(QVI_HWLOC_EXTRA_CONFIG_FLAGS "--enable-silent-rules=no")
endif()

set(PCIACCESS_NEEDED FALSE)

if(CUDAToolkit_FOUND AND QV_GPU_SUPPORT)
    list(APPEND QVI_HWLOC_GPU_FLAGS "--with-cuda=${CUDAToolkit_TARGET_DIR}")
    list(APPEND QVI_HWLOC_GPU_FLAGS "--enable-cuda=yes")
    list(APPEND QVI_HWLOC_GPU_FLAGS "--enable-nvml=yes")
    set(PCIACCESS_NEEDED TRUE)
else()
    list(APPEND QVI_HWLOC_GPU_FLAGS "--enable-cuda=no")
    list(APPEND QVI_HWLOC_GPU_FLAGS "--enable-nvml=no")
endif()

if(ROCmSMI_FOUND AND QV_GPU_SUPPORT)
    list(APPEND QVI_HWLOC_GPU_FLAGS "--enable-rsmi")
    set(QVI_HWLOC_CPPFLAGS "-I${ROCmSMI_INCLUDE_DIRS}")
    set(QVI_HWLOC_LDFLAGS "${ROCmSMI_LIBRARIES}")
    list(APPEND QVI_HWLOC_CONFIG_VARS "CPPFLAGS=${QVI_HWLOC_CPPFLAGS}")
    list(APPEND QVI_HWLOC_CONFIG_VARS "LDFLAGS=${QVI_HWLOC_LDFLAGS}")
    set(PCIACCESS_NEEDED TRUE)
else()
    list(APPEND QVI_HWLOC_GPU_FLAGS "--disable-rsmi")
endif()

if(PCIACCESS_NEEDED)
    find_package(PCIAccess REQUIRED)
    list(APPEND QVI_HWLOC_GPU_FLAGS "--enable-pci=yes")
else()
    list(APPEND QVI_HWLOC_GPU_FLAGS "--enable-pci=no")
endif()

message(STATUS "hwloc configure variables are:")
list(APPEND CMAKE_MESSAGE_INDENT "  ")
foreach(var ${QVI_HWLOC_CONFIG_VARS})
    message(STATUS "${var}")
endforeach()
list(POP_BACK CMAKE_MESSAGE_INDENT)

message(STATUS "hwloc GPU configure flags are:")
list(APPEND CMAKE_MESSAGE_INDENT "  ")
foreach(flag ${QVI_HWLOC_GPU_FLAGS})
    message(STATUS "${flag}")
endforeach()
list(POP_BACK CMAKE_MESSAGE_INDENT)

ExternalProject_Add(
    libhwloc
    URL file://${QVI_HWLOC_DIR}
    URL_MD5 "e9cb9230bcdf450b0948f255d505503f"
    PREFIX ${QVI_HWLOC_PREFIX}
    CONFIGURE_COMMAND
      <SOURCE_DIR>/configure
      ${QVI_HWLOC_CONFIG_VARS}
      --prefix=${QVI_HWLOC_PREFIX}
      --with-hwloc-symbol-prefix=quo_vadis_internal_
      --enable-static=yes
      --enable-shared=no
      --with-pic=yes
      --enable-plugins=no
      --enable-libxml2=no
      --enable-cairo=no
      --enable-gl=no
      --enable-opencl=no
      --enable-libudev=no
      ${QVI_HWLOC_EXTRA_CONFIG_FLAGS}
      ${QVI_HWLOC_GPU_FLAGS}
    BUILD_COMMAND ${QVI_HWLOC_BUILD_COMMAND}
    INSTALL_COMMAND ${QVI_HWLOC_BUILD_COMMAND} install
    BUILD_BYPRODUCTS ${QVI_HWLOC_STATIC_LIB}
)

add_library(hwloc STATIC IMPORTED GLOBAL)
add_dependencies(hwloc libhwloc)

set_target_properties(
    hwloc
    PROPERTIES
      IMPORTED_LOCATION ${QVI_HWLOC_STATIC_LIB}
      INTERFACE_INCLUDE_DIRECTORIES ${QVI_HWLOC_INCLUDES}
)

if(PCIACCESS_NEEDED)
    target_link_libraries(
        hwloc
        INTERFACE
          pciaccess
    )
endif()

if(CUDAToolkit_FOUND AND QV_GPU_SUPPORT)
    target_link_libraries(
        hwloc
        INTERFACE
          CUDA::cudart
          CUDA::nvml
    )
endif()

if(ROCmSMI_FOUND AND QV_GPU_SUPPORT)
    target_link_libraries(
        hwloc
        INTERFACE
          ROCmSMI
    )
endif()

# vim: ts=4 sts=4 sw=4 expandtab
