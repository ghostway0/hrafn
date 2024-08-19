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
  sources: files('multiaddr.h', 'semantic_version.h', 'uuid.h'),
  include_directories: [hrafn_inc],
)

test_utils_exe = executable('test_utils', 'test_utils.cpp', dependencies: [doctest_dep, utils_dep])
test('test_utils', test_utils_exe)
