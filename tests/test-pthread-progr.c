
/*
 * Async. Progress Thread of MPICH: Code
 */
#if 0
void MPL_thread_create(MPL_thread_func_t func, void *data,
               MPL_thread_id_t * idp, int *errp) {
  // ...
#if MPL_THREAD_PACKAGE_NAME == MPL_THREAD_PACKAGE_UTI
  uti_attr_t uti_attr;
  err = uti_attr_init(&uti_attr);
  if(err) { goto uti_exit; }

  /* Suggest that it's beneficial to put the thread
     on the same NUMA-domain as the caller */
  err = UTI_ATTR_SAME_NUMA_DOMAIN(&uti_attr);
  if(err) { goto uti_exit; }

  /* Suggest that the thread repeatedly monitors a device */
  err = UTI_ATTR_CPU_INTENSIVE(&uti_attr);
  if(err) { goto uti_exit; }

  err = uti_pthread_create(idp, &attr, MPLI_thread_start,
               thread_info, &uti_attr);
  if(err) { goto uti_exit; }

  err = uti_attr_destroy(&uti_attr);
  if(err) { goto uti_exit; }
 uti_exit:;
#else
  err = pthread_create(idp, &attr, MPLI_thread_start, thread_info);
#endif
  //...
}
#endif

/*
 * int pthread_create(pthread_t *restrict thread,
 *             const pthread_attr_t *restrict attr,
 *              void *(*start_routine)(void *),
 *              void *restrict arg);
 */


/*
 *  This example mimics a progress thread being launched
 *  from the MPI implementation.
 *  MPI can use the intrinsic scopes in QV to derive
 *  resources from as we do in this example.
 */
int MPI_thread_create(pthread_t *restrict thread,
              const pthread_attr_t *restrict attr,
          void *(*start_routine)(void *),
          void *restrict arg)
{
  char const *ers = NULL;
  MPI_Comm comm = MPI_COMM_WORLD;

  /* Initialization */
  int rc = MPI_Init(&argc, &argv);
  if (rc != MPI_SUCCESS) {
    ers = "MPI_Init() failed";
    panic("%s (rc=%d)", ers, rc);
  }

  /* Create a QV context */
  qv_context_t *ctx;
  rc = qv_mpi_create(&ctx, comm);
  if (rc != QV_SUCCESS) {
    ers = "qv_mpi_create() failed";
    panic("%s (rc=%s)", ers, qv_strerr(rc));
  }

  /* Get base scope: RM-given resources */
  qv_scope_t *base_scope;
  rc = qv_scope_get(ctx, QV_SCOPE_USER, &base_scope);
  if (rc != QV_SUCCESS) {
    ers = "qv_scope_get() failed";
    panic("%s (rc=%s)", ers, qv_strerr(rc));
  }

  /* Get a subscope to launch a progress thread */
  qv_scope_t *sub_scope;
  /* Need to discuss what attributes make sense, e.g.,
     "close to me" */
  int qv_attr = QV_SCOPE_ATTR_SAME_NUMA | QV_SCOPE_ATTR_EXCLUSIVE;
  rc = qv_scope_create(ctx, base_scope,
               QV_HW_OBJ_CORE, 1, qv_attr,
               &subscope);
  if (rc != QV_SUCCESS) {
    ers = "qv_scope_create() failed";
    panic("%s (rc=%s)", ers, qv_strerr(rc));
  }

  /* Two options here:
   * 1. Push into the sub_scope and then launch the thread
   * 2. Launch the thread with a spawn call that has an
   *    input scope and context as a parameter
   * The most general option is (2) since it does
   * not require to push into that scope. Plus there might
   * be some restrictions to push into a scope (say OS
   * scope), but one may be able to launch a new thread there.
   */

  // [...]

   qv_pthread_create(thread, attr, start_routine, arg,
                    subscope, ctx);

  /* Other systems may have calls like these: */
  // qv_mos_create_thread()
  // qv_mckernel_create_thread()

  return 0;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */