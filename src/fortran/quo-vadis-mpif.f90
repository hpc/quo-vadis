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
    function qv_mpi_scope_get_c(comm, iscope, scope) &
        bind(c, name='qvi_mpi_scope_get_f2c')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        integer, value :: comm
        integer(c_int), value :: iscope
        type(c_ptr), intent(out) :: scope
    end function qv_mpi_scope_get_c

    integer(c_int) &
    function qv_mpi_scope_comm_dup_c(scope, comm) &
        bind(c, name='qvi_mpi_scope_comm_dup_f2c')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer, intent(out) :: comm
    end function qv_mpi_scope_comm_dup_c

end interface

contains

    subroutine qv_mpi_scope_get(comm, iscope, scope, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        integer, value :: comm
        integer(c_int), value :: iscope
        type(c_ptr), intent(out) :: scope
        integer(c_int), intent(out) :: info
        info = qv_mpi_scope_get_c(comm, iscope, scope)
    end subroutine qv_mpi_scope_get

    subroutine qv_mpi_scope_comm_dup(scope, comm, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer, intent(out) :: comm
        integer(c_int), intent(out) :: info
        info =  qv_mpi_scope_comm_dup_c(scope, comm)
    end subroutine qv_mpi_scope_comm_dup

end module quo_vadis_mpif

! vim: ft=fortran ts=4 sts=4 sw=4 expandtab
