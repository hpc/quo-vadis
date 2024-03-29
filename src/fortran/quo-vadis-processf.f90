!
! Copyright (c) 2013-2022 Triad National Security, LLC
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
    function qv_process_context_create_c(ctx) &
        bind(c, name='qv_process_context_create')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), intent(out) :: ctx
    end function qv_process_context_create_c

    integer(c_int) &
    function qv_process_context_free_c(ctx) &
        bind(c, name='qv_process_context_free')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: ctx
    end function qv_process_context_free_c

end interface

contains

    subroutine qv_process_context_create(ctx, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), intent(out) :: ctx
        integer(c_int), intent(out) :: info
        info = qv_process_context_create_c(ctx)
    end subroutine qv_process_context_create

    subroutine qv_process_context_free(ctx, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: ctx
        integer(c_int), intent(out) :: info
        info = qv_process_context_free_c(ctx)
    end subroutine qv_process_context_free

end module quo_vadis_processf

! vim: ft=fortran ts=4 sts=4 sw=4 expandtab
