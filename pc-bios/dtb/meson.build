blobs = []

dtc = find_program('dtc', required: false)
foreach f : [
  'bamboo.dts',
  'canyonlands.dts',
  'petalogix-s3adsp1800.dts',
  'petalogix-ml605.dts',
]
  out = fs.replace_suffix(f, '.dtb')
  if dtc.found()
    custom_target(out,
        build_by_default: have_system,
        input: files(f),
        output: out,
        install: get_option('install_blobs'),
        install_dir: qemu_datadir / 'dtb',
        command: [ dtc, '-q', '-I', 'dts', '-O', 'dtb',
                        '-o', '@OUTPUT@', '@INPUT0@' ])
  else
    blobs += out
  endif
endforeach

if get_option('install_blobs')
  install_data(blobs, install_dir: qemu_datadir / 'dtb')
endif
