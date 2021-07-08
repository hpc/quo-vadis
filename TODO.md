# TODO

* Add quo-vadis-thread.h to house thread-specific calls (e.g.,
    qv_thread_scope_split()).
* Implement 'standalone' mode.
* qv_scope_split() should provide splits that minimize 'movement' as much as
    possible. This is especially important on NUMA architectures.
* A possible missing part of the API is providing a automatic distribution over
  a given resource, like in Quo.
  * Maybe something like qv_scope_task_elect()?
  * Another challenge is that the caller doesn't know the distribution of the
    tasks to resources. Could we provide an automatic grouping in split?
* Make thread-safe.
* Add qvi_test_ helpers for all tests.
* Add version getter in API.
* Add ThreadSanitizer support in test suite: `-fsanitize=thread`
* Add AddressSanitizer support in test suite.
* Add UndefinedBehaviorSanitizer support in test suite.
* Implement a qvid distribution service.
* Add --devel option to configure to enable pedantic flags, etc.
* Add valgrind and helgrind tests to suite.
