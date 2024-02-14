!
! Copyright (c) 2013-2024 Triad National Security, LLC
!                         All rights reserved.
!
! This file is part of the quo-vadis project. See the LICENSE file at the
! top-level directory of this distribution.
!

module quo_vadis_mpif
    use, intrinsic :: iso_c_binding
    use quo_vadisf

interface
    integer(c_int) &
    function qv_mpi_context_create_c(comm, ctx) &
        bind(c, name='qvi_mpi_context_create_f2c')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), intent(out) :: ctx
        integer, value :: comm
    end function qv_mpi_context_create_c

    integer(c_int) &
    function qv_mpi_scope_comm_dup_c(ctx, scope, comm) &
        bind(c, name='qvi_mpi_scope_comm_dup_f2c')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: ctx
        type(c_ptr), value :: scope
        integer, intent(out) :: comm
    end function qv_mpi_scope_comm_dup_c

    integer(c_int) &
    function qv_mpi_context_free_c(ctx) &
        bind(c, name='qv_mpi_context_free')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: ctx
    end function qv_mpi_context_free_c
end interface

contains

    subroutine qv_mpi_context_create(comm, ctx, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), intent(out) :: ctx
        integer, value :: comm
        integer(c_int), intent(out) :: info
        info = qv_mpi_context_create_c(comm, ctx)
    end subroutine qv_mpi_context_create

    subroutine qv_mpi_scope_comm_dup(ctx, scope, comm, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: ctx
        type(c_ptr), value :: scope
        integer, intent(out) :: comm
        integer(c_int), intent(out) :: info
        info =  qv_mpi_scope_comm_dup_c(ctx, scope, comm)
    end subroutine qv_mpi_scope_comm_dup

    subroutine qv_mpi_context_free(ctx, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: ctx
        integer(c_int), intent(out) :: info
        info = qv_mpi_context_free_c(ctx)
    end subroutine qv_mpi_context_free

end module quo_vadis_mpif

! vim: ft=fortran ts=4 sts=4 sw=4 expandtab
