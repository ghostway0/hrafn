project('hrafn', 'cpp', 'objcpp', 'objc', default_options: [])

cpp_args = ['-march=native', '-std=c++23']
add_project_arguments(cpp_args, language: 'cpp')

objcpp_args = ['-std=c++20']
add_project_arguments(objcpp_args, language: 'objcpp')

is_debug = get_option('debug')
is_release = not is_debug
cxx = meson.get_compiler('cpp')

asio_dep = dependency('asio')
sodium_dep = dependency('libsodium')
protobuf_dep = dependency('protobuf')
absl_dep = dependency('absl', modules: ['absl::strings', 'absl::time'])
fmt_dep = dependency('fmt')
spdlog_dep = dependency('spdlog')
doctest_dep = dependency('doctest')

hrafn_inc = include_directories('.')

protoc = find_program('protoc', required: true)
protobuf_gen = generator(
  protoc,
  output: ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
  arguments: [
    '--proto_path=@SOURCE_ROOT@/proto',
    '--cpp_out=@BUILD_DIR@',
    '@INPUT@',
  ],
)
proto_generated = protobuf_gen.process('proto/messages.proto')

subdir('crypto')
subdir('utils')
subdir('net')
subdir('btle')

# executable(
#   'hrafn',
#   files('src/hrafn.cpp'),
#   proto_generated,
#   dependencies: [
#     asio_dep,
#     sodium_dep,
#     absl_dep,
#     protobuf_dep,
#     fmt_dep,
#     spdlog_dep,
#     crypto_dep,
#     utils_dep,
#     btle_dep,
#   ],
#   include_directories: [hrafn_inc],
# )

executable(
  'bttest',
  files('src/test.cpp'),
  proto_generated,
  dependencies: [
    asio_dep,
    sodium_dep,
    absl_dep,
    protobuf_dep,
    fmt_dep,
    spdlog_dep,
    crypto_dep,
    utils_dep,
    btle_dep,
  ],
  include_directories: [hrafn_inc],
)
