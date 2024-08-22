cbtle_sources = files('bt.mm')

cc = meson.get_compiler('cpp')
cbtle_lib = static_library(
  'cbtle',
  cbtle_sources,
  install: true,
  dependencies: [],
  include_directories: [hrafn_inc],
)


corebluetooth_dep = dependency('CoreBluetooth')
foundation_dep = declare_dependency(
  link_args: ['-framework', 'Foundation']
)

macos_btle_dep = declare_dependency(
  link_with: cbtle_lib,
  sources: files('btle.h'),
  dependencies: [
    corebluetooth_dep,
    cxx.find_library('objc'),
    foundation_dep,
  ],
  include_directories: [hrafn_inc],
)

if host_machine.system() == 'darwin'
  btle_dep = macos_btle_dep
endif
