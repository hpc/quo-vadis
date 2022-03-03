!
! Copyright (c) 2013-2022 Triad National Security, LLC
!                         All rights reserved.
!
! This file is part of the quo-vadis project. See the LICENSE file at the
! top-level directory of this distribution.
!

module quo_vadisf
      use, intrinsic :: iso_c_binding
      !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      ! Return Codes
      !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      integer(c_int) QV_SUCCESS
      integer(c_int) QV_SUCCESS_ALREADY_DONE
      integer(c_int) QV_SUCCESS_SHUTDOWN
      integer(c_int) QV_ERR
      integer(c_int) QV_ERR_ENV
      integer(c_int) QV_ERR_INTERNAL
      integer(c_int) QV_ERR_FILE_IO
      integer(c_int) QV_ERR_SYS
      integer(c_int) QV_ERR_OOR
      integer(c_int) QV_ERR_INVLD_ARG
      integer(c_int) QV_ERR_CALL_BEFORE_INIT
      integer(c_int) QV_ERR_HWLOC
      integer(c_int) QV_ERR_MPI
      integer(c_int) QV_ERR_MSG
      integer(c_int) QV_ERR_RPC
      integer(c_int) QV_ERR_NOT_SUPPORTED
      integer(c_int) QV_ERR_POP
      integer(c_int) QV_ERR_PMI
      integer(c_int) QV_ERR_NOT_FOUND
      integer(c_int) QV_ERR_SPLIT
      integer(c_int) QV_RES_UNAVAILABLE
      integer(c_int) QV_RC_LAST

      parameter (QV_SUCCESS = 0)
      parameter (QV_SUCCESS_ALREADY_DONE = 1)
      parameter (QV_SUCCESS_SHUTDOWN = 2)
      parameter (QV_ERR = 3)
      parameter (QV_ERR_ENV = 4)
      parameter (QV_ERR_INTERNAL = 5)
      parameter (QV_ERR_FILE_IO = 6)
      parameter (QV_ERR_SYS = 7)
      parameter (QV_ERR_OOR = 8)
      parameter (QV_ERR_INVLD_ARG = 9)
      parameter (QV_ERR_CALL_BEFORE_INIT = 10)
      parameter (QV_ERR_HWLOC = 11)
      parameter (QV_ERR_MPI = 12)
      parameter (QV_ERR_MSG = 13)
      parameter (QV_ERR_RPC = 14)
      parameter (QV_ERR_NOT_SUPPORTED = 15)
      parameter (QV_ERR_POP = 16)
      parameter (QV_ERR_PMI = 17)
      parameter (QV_ERR_NOT_FOUND = 18)
      parameter (QV_ERR_SPLIT = 19)
      parameter (QV_RES_UNAVAILABLE = 20)
      parameter (QV_RC_LAST = 21)

      !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      ! Intrinsic Scopes
      !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      integer(c_int) QV_SCOPE_SYSTEM
      integer(c_int) QV_SCOPE_USER
      integer(c_int) QV_SCOPE_JOB
      integer(c_int) QV_SCOPE_PROCESS

      parameter (QV_SCOPE_SYSTEM = 0)
      parameter (QV_SCOPE_USER = 1)
      parameter (QV_SCOPE_JOB = 2)
      parameter (QV_SCOPE_PROCESS = 3)

      !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      ! Hardware Object Types
      !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      integer(c_int) QV_HW_OBJ_MACHINE
      integer(c_int) QV_HW_OBJ_PACKAGE
      integer(c_int) QV_HW_OBJ_CORE
      integer(c_int) QV_HW_OBJ_PU
      integer(c_int) QV_HW_OBJ_L1CACHE
      integer(c_int) QV_HW_OBJ_L2CACHE
      integer(c_int) QV_HW_OBJ_L3CACHE
      integer(c_int) QV_HW_OBJ_L4CACHE
      integer(c_int) QV_HW_OBJ_L5CACHE
      integer(c_int) QV_HW_OBJ_NUMANODE
      integer(c_int) QV_HW_OBJ_GPU
      integer(c_int) QV_HW_OBJ_LAST

      parameter (QV_HW_OBJ_MACHINE = 0)
      parameter (QV_HW_OBJ_PACKAGE = 1)
      parameter (QV_HW_OBJ_CORE = 2)
      parameter (QV_HW_OBJ_PU = 3)
      parameter (QV_HW_OBJ_L1CACHE = 4)
      parameter (QV_HW_OBJ_L2CACHE = 5)
      parameter (QV_HW_OBJ_L3CACHE = 6)
      parameter (QV_HW_OBJ_L4CACHE = 7)
      parameter (QV_HW_OBJ_L5CACHE = 8)
      parameter (QV_HW_OBJ_NUMANODE = 9)
      parameter (QV_HW_OBJ_GPU = 10)
      parameter (QV_HW_OBJ_LAST = 11)

      !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      ! Automatic grouping options for qv_scope_split().
      !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      integer(c_int) QV_SCOPE_SPLIT_AFFINITY_PRESERVING

      parameter (QV_SCOPE_SPLIT_AFFINITY_PRESERVING = -1)

      !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      ! Device ID types
      !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      integer(c_int) QV_DEVICE_ID_UUID
      integer(c_int) QV_DEVICE_ID_PCI_BUS_ID
      integer(c_int) QV_DEVICE_ID_ORDINAL

      parameter (QV_DEVICE_ID_UUID = 0)
      parameter (QV_DEVICE_ID_PCI_BUS_ID = 1)
      parameter (QV_DEVICE_ID_ORDINAL = 2)

interface
      integer(c_int) &
      function qv_scope_get_c(ctx, iscope, scope) &
          bind(c, name='qv_scope_get')
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          integer(c_int), value :: iscope
          type(c_ptr), intent(out) :: scope
      end function qv_scope_get_c
end interface

interface
      integer(c_int) &
      function qv_scope_free_c(ctx, scope) &
          bind(c, name='qv_scope_free')
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          type(c_ptr), value :: scope
      end function qv_scope_free_c
end interface

interface
      integer(c_int) &
      function qv_scope_split_c(&
          ctx, scope, npieces, group_id, subscope &
      ) &
          bind(c, name='qv_scope_split')
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          type(c_ptr), value :: scope
          integer(c_int), value :: npieces
          integer(c_int), value :: group_id
          type(c_ptr), intent(out) :: subscope
      end function qv_scope_split_c
end interface

interface
      integer(c_int) &
      function qv_scope_split_at_c(&
          ctx, scope, obj_type, group_id, subscope &
      ) &
          bind(c, name='qv_scope_split_at')
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          type(c_ptr), value :: scope
          integer(c_int), value :: obj_type
          integer(c_int), value :: group_id
          type(c_ptr), intent(out) :: subscope
      end function qv_scope_split_at_c
end interface

interface
      integer(c_int) &
      function qv_scope_nobjs_c(ctx, scope, obj, n) &
          bind(c, name='qv_scope_nobjs')
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          type(c_ptr), value :: scope
          integer(c_int), value :: obj
          integer(c_int), intent(out) :: n
      end function qv_scope_nobjs_c
end interface

interface
      integer(c_int) &
      function qv_scope_taskid_c(ctx, scope, taskid) &
          bind(c, name='qv_scope_taskid')
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          type(c_ptr), value :: scope
          integer(c_int), intent(out) :: taskid
      end function qv_scope_taskid_c
end interface

interface
      integer(c_int) &
      function qv_scope_ntasks_c(ctx, scope, ntasks) &
          bind(c, name='qv_scope_ntasks')
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          type(c_ptr), value :: scope
          integer(c_int), intent(out) :: ntasks
      end function qv_scope_ntasks_c
end interface

interface
      integer(c_int) &
      function qv_scope_barrier_c(ctx, scope) &
          bind(c, name='qv_scope_barrier')
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          type(c_ptr), value :: scope
      end function qv_scope_barrier_c
end interface

! TODO(skg) Add qv_scope_get_device()

interface
      integer(c_int) &
      function qv_bind_push_c(ctx, scope) &
          bind(c, name='qv_bind_push')
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          type(c_ptr), value :: scope
      end function qv_bind_push_c
end interface

interface
      integer(c_int) &
      function qv_bind_pop_c(ctx) &
          bind(c, name='qv_bind_pop')
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
      end function qv_bind_pop_c
end interface

! TODO(skg) Add qv_bind_get_as_string()

interface
      integer(c_int) &
      function qv_context_barrier_c(ctx) &
          bind(c, name='qv_context_barrier')
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
      end function qv_context_barrier_c
end interface

contains

      subroutine qv_scope_get(ctx, iscope, scope, info)
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          integer(c_int), value :: iscope
          type(c_ptr), intent(out) :: scope
          integer(c_int), intent(out) :: info
          info = qv_scope_get_c(ctx, iscope, scope)
      end subroutine qv_scope_get

      subroutine qv_scope_free(ctx, scope, info)
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          type(c_ptr), value :: scope
          integer(c_int), intent(out) :: info
          info = qv_scope_free_c(ctx, scope)
      end subroutine qv_scope_free

      subroutine qv_scope_split( &
              ctx, scope, npieces, group_id, subscope, info &
      )
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          type(c_ptr), value :: scope
          integer(c_int), value :: npieces
          integer(c_int), value :: group_id
          type(c_ptr), intent(out) :: subscope
          integer(c_int), intent(out) :: info
          info = qv_scope_split_c( &
              ctx, scope, npieces, group_id, subscope &
          )
      end subroutine qv_scope_split

      subroutine qv_scope_split_at(&
          ctx, scope, obj_type, group_id, subscope, info &
      )
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          type(c_ptr), value :: scope
          integer(c_int), value :: obj_type
          integer(c_int), value :: group_id
          type(c_ptr), intent(out) :: subscope
          integer(c_int), intent(out) :: info
          info = qv_scope_split_at_c( &
              ctx, scope, obj_type, group_id, subscope &
          )
      end subroutine qv_scope_split_at

      subroutine qv_scope_nobjs(ctx, scope, obj, n, info)
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          type(c_ptr), value :: scope
          integer(c_int), value :: obj
          integer(c_int), intent(out) :: n
          integer(c_int), intent(out) :: info
          info = qv_scope_nobjs_c(ctx, scope, obj, n)
      end subroutine qv_scope_nobjs

      subroutine qv_scope_taskid(ctx, scope, taskid, info)
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          type(c_ptr), value :: scope
          integer(c_int), intent(out) :: taskid
          integer(c_int), intent(out) :: info
          info = qv_scope_taskid_c(ctx, scope, taskid)
      end subroutine qv_scope_taskid

      subroutine qv_scope_ntasks(ctx, scope, ntasks, info)
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          type(c_ptr), value :: scope
          integer(c_int), intent(out) :: ntasks
          integer(c_int), intent(out) :: info
          info = qv_scope_ntasks_c(ctx, scope, ntasks)
      end subroutine qv_scope_ntasks

      subroutine qv_scope_barrier(ctx, scope, info)
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          type(c_ptr), value :: scope
          integer(c_int), intent(out) :: info
          info = qv_scope_barrier_c(ctx, scope)
      end subroutine qv_scope_barrier

      ! TODO(skg) Add qv_scope_get_device()

      subroutine qv_bind_push(ctx, scope, info)
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          type(c_ptr), value :: scope
          integer(c_int), intent(out) :: info
          info = qv_bind_push_c(ctx, scope)
      end subroutine qv_bind_push

      subroutine qv_bind_pop(ctx, info)
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          integer(c_int), intent(out) :: info
          info = qv_bind_pop_c(ctx)
      end subroutine qv_bind_pop

      ! TODO(skg) Add qv_bind_get_as_string()

      subroutine qv_context_barrier(ctx, info)
          use, intrinsic :: iso_c_binding, only: c_ptr, c_int
          implicit none
          type(c_ptr), value :: ctx
          integer(c_int), intent(out) :: info
          info = qv_context_barrier_c(ctx)
      end subroutine qv_context_barrier

end module quo_vadisf
