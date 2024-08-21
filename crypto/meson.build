crypto_sources = files(
  'crc64.cpp',
  'crypto.cpp',
)

crypto_lib = static_library(
  'crypto',
  crypto_sources,
  include_directories: [hrafn_inc],
  install: true,
  dependencies: [absl_dep],
)

crypto_dep = declare_dependency(
  link_with: crypto_lib,
  sources: files(
    'crc64.h',
    'crypto.h',
  ),
  include_directories: [hrafn_inc],
)

test_crc_exe = executable('test_crc', 'test_crc64.cpp', dependencies: [doctest_dep, crypto_dep])
test('test_crc64', test_crc_exe)
