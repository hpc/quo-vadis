# TODO

* Fix spdlog build: uses GCC even if Clang is specified at top level.
* Implement transparent loading of synthetic hardware topologies for development
  purposes.
* Implement a qvid distribution service.
* Figure out how we are going to deal with client/server connection information.
  One possibility is to provide environmental controls (e.g., an environment
  variable that specifies the connection information). I'm not a huge fan of
  this, but it's certainly the easiest route.
    - Parse /proc/net/tcp?
    - https://zguide.zeromq.org/docs/chapter8/#Network-Discovery
* Focus on writing a test that demonstrates hierarchical RMI.
* Improve logger flushing.
* Consider turning off C++ exceptions.
* Add --devel option to configure to enable pedantic flags, etc.
* Add valgrind and helgrind tests to suite.
