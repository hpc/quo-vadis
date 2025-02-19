! Does nothing useful. Just used to exercise the Fortran interface.

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
program process_fortapi

    use quo_vadis_processf
    use, intrinsic :: iso_c_binding
    implicit none


    integer(c_int) info, n
    integer(c_int) sgsize, sgrank, n_cores, n_gpu
    type(c_ptr) scope_user
    character(len=:),allocatable :: bstr(:)
    character(len=:),allocatable :: dev_pci(:)
    character, pointer, dimension(:) :: strerr

    call qv_process_scope_get(QV_SCOPE_USER, scope_user, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if

    call qv_scope_group_size(scope_user, sgsize, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if
    print *, 'sgsize', sgsize

    call qv_scope_group_rank(scope_user, sgrank, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if
    print *, 'sgrank', sgrank

    call qv_scope_nobjs(scope_user, QV_HW_OBJ_CORE, n_cores, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if
    print *, 'ncores', n_cores

    call qv_scope_bind_string(scope_user, QV_BIND_STRING_LOGICAL, bstr, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if
    print *, 'bstr ', bstr
    deallocate(bstr)

    call qv_scope_nobjs(scope_user, QV_HW_OBJ_GPU, n_gpu, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if
    print *, 'ngpu', n_gpu

    do n = 0, n_gpu - 1
        call qv_scope_get_device_id( &
            scope_user, QV_HW_OBJ_GPU, n, &
            QV_DEVICE_ID_PCI_BUS_ID, dev_pci, info &
        )
        if (info .ne. QV_SUCCESS) then
            error stop
        end if
        print *, 'dev_pci ', n, dev_pci
        deallocate(dev_pci)
    end do

    print *, 'testing qv_strerr'
    strerr => qv_strerr(QV_SUCCESS)
    print *, 'success is ', strerr

    strerr => qv_strerr(QV_ERR_OOR)
    print *, 'err oor is ', strerr

    call qv_scope_free(scope_user, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if

end program process_fortapi

! vim: ft=fortran ts=4 sts=4 sw=4 expandtab
