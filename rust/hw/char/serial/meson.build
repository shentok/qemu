_libserial_bindings_inc_rs = rust.bindgen(
  args: bindgen_args_common + [
    '--allowlist-file',
      meson.project_source_root() /
      'include/hw/char/serial.h|include/qemu/fifo8.h',
    '--blocklist-file',
      meson.project_source_root() /
      'include/(block\|chardev/|exec/|hw/core/|qom/|system/).*',
  ],
  kwargs: bindgen_kwargs,
)

_libserial_sys_rs = static_library(
  'serial_sys',
  structured_sources(
    [
      'src/sys/lib.rs',
      'src/sys/bindings.rs',
    ],
    {'.' : _libserial_bindings_inc_rs},
  ),
  override_options: ['rust_std=2021', 'build.rust_std=2021'],
  rust_abi: 'rust',
  dependencies: [
    chardev_sys_rs,
    glib_sys_rs,
    hwcore_sys_rs,
    system_sys_rs,
    util_sys_rs,
  ],
)

_libserial_rs = static_library(
  'serial',
  sources: [
    'src/lib.rs',
    'src/device.rs',
    'src/registers.rs',
  ],
  override_options: ['rust_std=2021', 'build.rust_std=2021'],
  rust_abi: 'rust',
  link_with: [
    _libserial_sys_rs,
    _util_rs,
    _migration_rs,
    _bql_rs,
    _qom_rs,
    _chardev_rs,
    _system_rs,
    _hwcore_rs,
    _trace_rs
  ],
  dependencies: [
    bilge_rs,
    bilge_impl_rs,
    bits_rs,
    common_rs,
    glib_sys_rs,
  ],
)

rust_devices_ss.add(when: 'CONFIG_X_SERIAL_RUST', if_true: [declare_dependency(
  link_whole: [_libserial_rs],
  variables: {'crate': 'serial'},
)])
