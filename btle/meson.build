btle_sources = []
btle_deps = []

if host_machine.system() == 'darwin'
  corebluetooth_dep = dependency('CoreBluetooth')
  foundation_dep = declare_dependency(link_args: ['-framework', 'Foundation'])

  btle_deps = [corebluetooth_dep, foundation_dep, cxx.find_library('objc')]
  btle_sources = files('corebluetooth/bt.mm', 'corebluetooth/cbtle.cpp')
elif host_machine.system() == 'linux'
  sdbusplus_dep = dependency('sdbusplus')

  btle_deps = [sdbusplus_dep]
  btle_sources = files()
endif

btle_lib = static_library(
  'btle',
  btle_sources,
  install: true,
  dependencies: [
    absl_dep,
  ] + btle_deps,
  include_directories: [hrafn_inc],
)

btle_dep = declare_dependency(
  link_with: btle_lib,
  sources: files('btle.h', 'types.h'),
  dependencies: [],
  include_directories: [hrafn_inc],
)
