rust = import('unstable-rust')

generated = rust.bindgen(
  input : '../include/qom/object.h',
  output : 'object-sys.rs',
  include_directories : [target_inc],
)

qom_ss.add(genh)
qom_ss.add(files(
  'container.c',
  'object.c',
  'object_interfaces.c',
  'qom-qobject.c',
))
qom_ss.add(generated)

qmp_ss.add(files('qom-qmp-cmds.c'))
softmmu_ss.add(files('qom-hmp-cmds.c'))
