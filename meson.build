project('hrafn', 'cpp', default_options: ['cpp_std=c++23'])

cpp_args = ['-march=native']

is_debug = get_option('debug')
is_release = not is_debug

asio_dep = dependency('asio')
sodium_dep = dependency('libsodium')
protobuf_dep = dependency('protobuf')
absl_dep = dependency('absl', modules: ['absl::strings'])
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

executable(
  'hrafn',
  files('src/main.cpp'),
  proto_generated,
  cpp_args: cpp_args,
  dependencies: [
    asio_dep,
    sodium_dep,
    absl_dep,
    protobuf_dep,
    fmt_dep,
    spdlog_dep,
    crypto_dep,
    utils_dep,
  ],
  include_directories: [hrafn_inc],
)
