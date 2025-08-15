!
! Copyright (c) 2013-2025 Triad National Security, LLC
!                         All rights reserved.
!
! This file is part of the quo-vadis project. See the LICENSE file at the
! top-level directory of this distribution.
!

module quo_vadisf
    use, intrinsic :: iso_c_binding
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    ! Return codes
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    integer(c_int), parameter :: QV_SUCCESS = 0
    integer(c_int), parameter :: QV_SUCCESS_ALREADY_DONE = 1
    integer(c_int), parameter :: QV_SUCCESS_SHUTDOWN = 2
    integer(c_int), parameter :: QV_ERR = 3
    integer(c_int), parameter :: QV_ERR_ENV = 4
    integer(c_int), parameter :: QV_ERR_INTERNAL = 5
    integer(c_int), parameter :: QV_ERR_FILE_IO = 6
    integer(c_int), parameter :: QV_ERR_SYS = 7
    integer(c_int), parameter :: QV_ERR_OOR = 8
    integer(c_int), parameter :: QV_ERR_INVLD_ARG = 9
    integer(c_int), parameter :: QV_ERR_CALL_BEFORE_INIT = 10
    integer(c_int), parameter :: QV_ERR_HWLOC = 11
    integer(c_int), parameter :: QV_ERR_MPI = 12
    integer(c_int), parameter :: QV_ERR_MSG = 13
    integer(c_int), parameter :: QV_ERR_RPC = 14
    integer(c_int), parameter :: QV_ERR_NOT_SUPPORTED = 15
    integer(c_int), parameter :: QV_ERR_POP = 16
    integer(c_int), parameter :: QV_ERR_NOT_FOUND = 18
    integer(c_int), parameter :: QV_ERR_SPLIT = 19
    integer(c_int), parameter :: QV_RES_UNAVAILABLE = 20

    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    ! Intrinsic scopes
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    integer(c_int), parameter :: QV_SCOPE_SYSTEM = 0
    integer(c_int), parameter :: QV_SCOPE_USER = 1
    integer(c_int), parameter :: QV_SCOPE_JOB = 2
    integer(c_int), parameter :: QV_SCOPE_PROCESS = 3

    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    ! Intrinsic scope flags
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    integer(c_long_long), parameter :: QV_SCOPE_FLAG_NONE = &
        int(ishft(0, 0), kind=c_long_long)

    integer(c_long_long), parameter :: QV_SCOPE_FLAG_NO_SMT = &
        int(ishft(1, 0), kind=c_long_long)

    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    ! Hardware Object Types
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    integer(c_int), parameter :: QV_HW_OBJ_MACHINE = 0
    integer(c_int), parameter :: QV_HW_OBJ_PACKAGE = 1
    integer(c_int), parameter :: QV_HW_OBJ_CORE = 2
    integer(c_int), parameter :: QV_HW_OBJ_PU = 3
    integer(c_int), parameter :: QV_HW_OBJ_L1CACHE = 4
    integer(c_int), parameter :: QV_HW_OBJ_L2CACHE = 5
    integer(c_int), parameter :: QV_HW_OBJ_L3CACHE = 6
    integer(c_int), parameter :: QV_HW_OBJ_L4CACHE = 7
    integer(c_int), parameter :: QV_HW_OBJ_L5CACHE = 8
    integer(c_int), parameter :: QV_HW_OBJ_NUMANODE = 9
    integer(c_int), parameter :: QV_HW_OBJ_GPU = 10
    integer(c_int), parameter :: QV_HW_OBJ_LAST = 11

    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    ! Binding string representation format flags.
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    integer(c_int), parameter :: QV_BIND_STRING_LOGICAL = &
        int(ishft(1, 0), kind=c_int)

    integer(c_int), parameter :: QV_BIND_STRING_PHYSICAL = &
        int(ishft(1, 1), kind=c_int)

    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    ! Automatic grouping options for qv_scope_split().
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    integer(c_int), parameter :: QV_SCOPE_SPLIT_UNDEFINED = -1
    integer(c_int), parameter :: QV_SCOPE_SPLIT_AFFINITY_PRESERVING = -2

    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    ! Device ID types
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    integer(c_int), parameter :: QV_DEVICE_ID_UUID = 0
    integer(c_int), parameter :: QV_DEVICE_ID_PCI_BUS_ID = 1
    integer(c_int), parameter :: QV_DEVICE_ID_ORDINAL = 2

interface
    pure function qvif_strlen_c(s) &
        bind(c, name="strlen")
        use, intrinsic :: iso_c_binding
        implicit none
        type(c_ptr), intent(in), value :: s
        integer(c_size_t) qvif_strlen_c
    end

    integer(c_int) &
    function qv_version_c(major, minor, patch) &
        bind(c, name='qv_version')
        use, intrinsic :: iso_c_binding, only: c_int
        implicit none
        integer(c_int), intent(out) :: major
        integer(c_int), intent(out) :: minor
        integer(c_int), intent(out) :: patch
    end function qv_version_c

    integer(c_int) &
    function qv_process_scope_get_c(iscope, flags, scope) &
        bind(c, name='qv_process_scope_get')
        use, intrinsic :: iso_c_binding, only: c_int, c_long_long, c_ptr
        implicit none
        integer(c_int), value :: iscope
        integer(c_long_long), value :: flags
        type(c_ptr), intent(out) :: scope
    end function qv_process_scope_get_c

    integer(c_int) &
    function qv_scope_free_c(scope) &
        bind(c, name='qv_scope_free')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
    end function qv_scope_free_c

    integer(c_int) &
    function qv_scope_split_c( &
        scope, npieces, group_id, subscope &
    ) &
        bind(c, name='qv_scope_split')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), value :: npieces
        integer(c_int), value :: group_id
        type(c_ptr), intent(out) :: subscope
    end function qv_scope_split_c

    integer(c_int) &
    function qv_scope_split_at_c( &
        scope, obj_type, group_id, subscope &
    ) &
        bind(c, name='qv_scope_split_at')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), value :: obj_type
        integer(c_int), value :: group_id
        type(c_ptr), intent(out) :: subscope
    end function qv_scope_split_at_c

    integer(c_int) &
    function qv_scope_nobjs_c(scope, obj, n) &
        bind(c, name='qv_scope_nobjs')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), value :: obj
        integer(c_int), intent(out) :: n
    end function qv_scope_nobjs_c

    integer(c_int) &
    function qv_scope_group_rank_c(scope, taskid) &
        bind(c, name='qv_scope_group_rank')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), intent(out) :: taskid
    end function qv_scope_group_rank_c

    integer(c_int) &
    function qv_scope_group_size_c(scope, ntasks) &
        bind(c, name='qv_scope_group_size')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), intent(out) :: ntasks
    end function qv_scope_group_size_c

    integer(c_int) &
    function qv_scope_barrier_c(scope) &
        bind(c, name='qv_scope_barrier')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
    end function qv_scope_barrier_c

    integer(c_int) &
    function qv_scope_device_id_get_c( &
        scope, dev_obj, i, id_type, dev_id &
    ) &
        bind(c, name='qv_scope_device_id_get')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), value :: dev_obj
        integer(c_int), value :: i
        integer(c_int), value :: id_type
        type(c_ptr), intent(out) :: dev_id
    end function qv_scope_device_id_get_c

    integer(c_int) &
    function qv_scope_bind_push_c(scope) &
        bind(c, name='qv_scope_bind_push')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
    end function qv_scope_bind_push_c

    integer(c_int) &
    function qv_scope_bind_pop_c(scope) &
        bind(c, name='qv_scope_bind_pop')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
    end function qv_scope_bind_pop_c

    integer(c_int) &
    function qv_scope_bind_string_c(scope, sformat, str) &
        bind(c, name='qv_scope_bind_string')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), value :: sformat
        type(c_ptr), intent(out) :: str
    end function qv_scope_bind_string_c

    type(c_ptr) &
    function qv_strerr_c(ec) &
        bind(c, name='qv_strerr')
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        integer(c_int), value :: ec
    end function qv_strerr_c

    subroutine qvif_free_c(p) &
        bind(c, name="free")
        use, intrinsic :: iso_c_binding
        type(c_ptr), intent(in), value :: p
    end subroutine qvif_free_c
end interface

contains

    function qv_strerr(ec) result(fstrp)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        integer(c_int), value :: ec
        character, pointer, dimension(:) :: fstrp

        type(c_ptr) :: cstr

        cstr = qv_strerr_c(ec)
        ! Now deal with the string
        call c_f_pointer(cstr, fstrp, [qvif_strlen_c(cstr)])
    end function qv_strerr

    subroutine qv_version(major, minor, patch, info)
        use, intrinsic :: iso_c_binding, only: c_int
        implicit none
        integer(c_int), intent(out) :: major
        integer(c_int), intent(out) :: minor
        integer(c_int), intent(out) :: patch
        integer(c_int), intent(out) :: info
        info = qv_version_c(major, minor, patch)
    end subroutine qv_version

    subroutine qv_process_scope_get(iscope, flags, scope, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_long_long, c_int
        implicit none
        integer(c_int), value :: iscope
        integer(c_long_long), value :: flags
        type(c_ptr), intent(out) :: scope
        integer(c_int), intent(out) :: info
        info = qv_process_scope_get_c(iscope, flags, scope)
    end subroutine qv_process_scope_get

    subroutine qv_scope_free(scope, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), intent(out) :: info
        info = qv_scope_free_c(scope)
    end subroutine qv_scope_free

    subroutine qv_scope_split( &
        scope, npieces, group_id, subscope, info &
    )
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), value :: npieces
        integer(c_int), value :: group_id
        type(c_ptr), intent(out) :: subscope
        integer(c_int), intent(out) :: info
        info = qv_scope_split_c( &
            scope, npieces, group_id, subscope &
        )
    end subroutine qv_scope_split

    subroutine qv_scope_split_at( &
        scope, obj_type, group_id, subscope, info &
    )
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), value :: obj_type
        integer(c_int), value :: group_id
        type(c_ptr), intent(out) :: subscope
        integer(c_int), intent(out) :: info
        info = qv_scope_split_at_c( &
            scope, obj_type, group_id, subscope &
        )
    end subroutine qv_scope_split_at

    subroutine qv_scope_nobjs(scope, obj, n, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), value :: obj
        integer(c_int), intent(out) :: n
        integer(c_int), intent(out) :: info
        info = qv_scope_nobjs_c(scope, obj, n)
    end subroutine qv_scope_nobjs

    subroutine qv_scope_group_rank(scope, taskid, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), intent(out) :: taskid
        integer(c_int), intent(out) :: info
        info = qv_scope_group_rank_c(scope, taskid)
    end subroutine qv_scope_group_rank

    subroutine qv_scope_group_size(scope, ntasks, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), intent(out) :: ntasks
        integer(c_int), intent(out) :: info
        info = qv_scope_group_size_c(scope, ntasks)
    end subroutine qv_scope_group_size

    subroutine qv_scope_barrier(scope, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), intent(out) :: info
        info = qv_scope_barrier_c(scope)
    end subroutine qv_scope_barrier

    subroutine qv_scope_device_id_get( &
        scope, dev_obj, i, id_type, dev_id, info &
    )
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), value :: dev_obj
        integer(c_int), value :: i
        integer(c_int), value :: id_type
        character(len=:), allocatable, intent(out) :: dev_id(:)
        integer(c_int), intent(out) :: info

        type(c_ptr) :: cstr
        integer(c_size_t) :: strlen
        character, pointer, dimension(:) :: fstrp

        info = qv_scope_device_id_get_c( &
            scope, dev_obj, i, id_type, cstr &
        )
        ! Now deal with the string
        strlen = qvif_strlen_c(cstr)
        call c_f_pointer(cstr, fstrp, [strlen])
        allocate(character(strlen) :: dev_id(1))
        dev_id = fstrp
        call qvif_free_c(cstr)
    end subroutine qv_scope_device_id_get

    subroutine qv_scope_bind_push(scope, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), intent(out) :: info
        info = qv_scope_bind_push_c(scope)
    end subroutine qv_scope_bind_push

    subroutine qv_scope_bind_pop(scope, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), intent(out) :: info
        info = qv_scope_bind_pop_c(scope)
    end subroutine qv_scope_bind_pop

    subroutine qv_scope_bind_string(scope, sformat, fstr, info)
        use, intrinsic :: iso_c_binding, only: c_ptr, c_int, c_size_t
        implicit none
        type(c_ptr), value :: scope
        integer(c_int), value :: sformat
        character(len=:), allocatable, intent(out) :: fstr(:)
        integer(c_int), intent(out) :: info

        type(c_ptr) :: cstr
        integer(c_size_t) :: strlen
        character, pointer, dimension(:) :: fstrp

        info = qv_scope_bind_string_c(scope, sformat, cstr)
        ! Now deal with the string
        strlen = qvif_strlen_c(cstr)
        call c_f_pointer(cstr, fstrp, [strlen])
        allocate(character(strlen) :: fstr(1))
        fstr = fstrp
        call qvif_free_c(cstr)
    end subroutine

end module quo_vadisf

! vim: ft=fortran ts=4 sts=4 sw=4 expandtab
