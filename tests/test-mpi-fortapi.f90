!
! Copyright (c) 2013-2024 Triad National Security, LLC
!                         All rights reserved.
!
! This file is part of the quo-vadis project. See the LICENSE file at the
! top-level directory of this distribution.
!

! Does nothing useful. Just used to exercise the Fortran interface.

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
program mpi_fortapi

    use mpi
    use quo_vadis_mpif
    use, intrinsic :: iso_c_binding
    implicit none


    integer(c_int) info, n
    integer(c_int) sgsize, sgrank, n_cores, n_gpu
    integer(c_int) vmajor, vminor, vpatch
    integer cwrank, cwsize, scope_comm, scope_comm_size
    type(c_ptr) scope_user, sub_scope
    character(len=:),allocatable :: bstr(:)
    character(len=:),allocatable :: dev_pci(:)
    character, pointer, dimension(:) :: strerr

    call mpi_init(info)
    if (info .ne. MPI_SUCCESS) then
        error stop
    end if

    call mpi_comm_rank(MPI_COMM_WORLD, cwrank, info)
    if (info .ne. MPI_SUCCESS) then
        error stop
    end if

    call mpi_comm_size(MPI_COMM_WORLD, cwsize, info)
    if (info .ne. MPI_SUCCESS) then
        error stop
    end if

    if (cwrank .eq. 0) then
        call qv_version(vmajor, vminor, vpatch, info)
        if (info .ne. QV_SUCCESS) then
            error stop
        end if
        print *, 'qv_version_major', vmajor
        print *, 'qv_version_minor', vminor
        print *, 'qv_version_patch', vpatch
        print *, 'cwsize', cwsize
    end if

    call qv_mpi_scope_get(MPI_COMM_WORLD, QV_SCOPE_USER, scope_user, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if

    call qv_mpi_scope_comm_dup(scope_user, scope_comm, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if

    call mpi_comm_size(scope_comm, scope_comm_size, info)
    if (info .ne. MPI_SUCCESS) then
        error stop
    end if

    if (cwrank .eq. 0) then
        print *, 'scope_comm_size', scope_comm_size
    end if

    if (scope_comm_size .ne. cwsize) then
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

    call qv_scope_split(scope_user, 2, sgrank, sub_scope, info)

    call qv_scope_bind_push(sub_scope, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if

    call qv_scope_bind_string(sub_scope, QV_BIND_STRING_AS_LIST, bstr, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if
    print *, 'bstr ', bstr
    deallocate(bstr)

    call qv_scope_bind_pop(sub_scope, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if

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

    call qv_scope_free(sub_scope, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if

    call mpi_comm_free(scope_comm, info)
    if (info .ne. MPI_SUCCESS) then
        error stop
    end if

    call mpi_finalize(info)
    if (info .ne. MPI_SUCCESS) then
        error stop
    end if

end program mpi_fortapi

! vim: ft=fortran ts=4 sts=4 sw=4 expandtab
