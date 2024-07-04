!
! Copyright (c) 2013-2024 Triad National Security, LLC
!                         All rights reserved.
!
! This file is part of the quo-vadis project. See the LICENSE file at the
! top-level directory of this distribution.
!

module quo_vadis_processf
    use, intrinsic :: iso_c_binding
    use quo_vadisf

interface

    integer(c_int) &
    function qv_process_scope_get_c(iscope, scope) &
        bind(c, name='qv_process_scope_get')
        use, intrinsic :: iso_c_binding, only: c_int, c_ptr
        implicit none
        integer(c_int), value :: iscope
        type(c_ptr), intent(out) :: scope
    end function qv_process_scope_get_c

end interface

contains

    subroutine qv_process_scope_get(iscope, scope, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        integer(c_int), value :: iscope
        type(c_ptr), intent(out) :: scope
        integer(c_int), intent(out) :: info
        info = qv_process_scope_get_c(iscope, scope)
    end subroutine qv_process_scope_get

end module quo_vadis_processf

! vim: ft=fortran ts=4 sts=4 sw=4 expandtab
