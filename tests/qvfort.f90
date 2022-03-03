!
! Copyright (c) 2013-2022 Triad National Security, LLC
!                         All rights reserved.
!
! This file is part of the quo-vadis project. See the LICENSE file at the
! top-level directory of this distribution.
!

! Does nothing useful. Just used to exercise the Fortran interface.

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
program qvfort

    use mpi
    use quo_vadis_mpif
    use, intrinsic :: iso_c_binding
    implicit none


    integer(c_int) info
    integer(c_int) ntasks, taskid, n_cores
    integer(c_int) cwrank
    type(c_ptr) ctx, scope_user

    call mpi_init(info)
    if (info .ne. MPI_SUCCESS) then
        error stop
    end if

    call mpi_comm_rank(MPI_COMM_WORLD, cwrank, info)
    if (info .ne. MPI_SUCCESS) then
        error stop
    end if

    call qv_mpi_context_create(ctx, MPI_COMM_WORLD, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if

    call qv_scope_get(ctx, QV_SCOPE_USER, scope_user, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if

    call qv_scope_ntasks(ctx, scope_user, ntasks, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if
    print *, 'ntasks', ntasks

    call qv_scope_taskid(ctx, scope_user, taskid, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if
    print *, 'taskid', taskid

    call qv_scope_nobjs(ctx, scope_user, QV_HW_OBJ_CORE, n_cores, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if
    print *, 'ncores', n_cores

    call qv_scope_free(ctx, scope_user, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if

    call qv_mpi_context_free(ctx, info)
    if (info .ne. QV_SUCCESS) then
        error stop
    end if

    call mpi_finalize(info)
    if (info .ne. MPI_SUCCESS) then
        error stop
    end if

end program qvfort
