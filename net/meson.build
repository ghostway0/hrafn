net_lib = static_library(
  'net',
  files(),
  include_directories: [hrafn_inc],
  install: true,
  dependencies: [asio_dep],
)

net_dep = declare_dependency(
  link_with: net_lib,
  sources: files('net.h'),
  include_directories: [hrafn_inc],
)
