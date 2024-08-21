utils_sources = files('multiaddr.cpp', 'semantic_version.cpp', 'uuid.cpp')

utils_lib = static_library(
  'utils',
  utils_sources,
  install: true,
  dependencies: [
    asio_dep,
    sodium_dep,
    absl_dep,
    protobuf_dep,
    fmt_dep,
    spdlog_dep,
  ],
  include_directories: [hrafn_inc],
)

utils_dep = declare_dependency(
  link_with: utils_lib,
  sources: files('multiaddr.h', 'semantic_version.h', 'uuid.h', 'varint.h'),
  include_directories: [hrafn_inc],
)

test_semver_exe = executable('test_semver', 'test_semver.cpp', dependencies: [doctest_dep, utils_dep])
test('test_semver', test_semver_exe)

test_multiaddr_exe = executable('test_multiaddr', 'test_multiaddr.cpp', dependencies: [doctest_dep, utils_dep])
test('test_multiaddr', test_multiaddr_exe)

test_uuid_exe = executable('test_uuid', 'test_uuid.cpp', dependencies: [doctest_dep, utils_dep])
test('test_uuid', test_uuid_exe)
