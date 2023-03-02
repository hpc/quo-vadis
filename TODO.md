# TODO

* Implement 'standalone' mode.
* qv_scope_split() should provide splits that minimize 'movement' as much as
    possible. This is especially important on NUMA architectures.
* Make thread-safe.
* Add ThreadSanitizer support in test suite: `-fsanitize=thread`
* Add AddressSanitizer support in test suite.
* Add UndefinedBehaviorSanitizer support in test suite.
* Implement a qvid distribution service.
* Add --devel option to configure to enable pedantic flags, etc.
* Add valgrind and helgrind tests to suite.
