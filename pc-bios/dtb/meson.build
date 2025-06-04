dts_ss = ss.source_set()

dts_ss.add(when: 'CONFIG_PPC440', if_true: files('bamboo.dts'))
dts_ss.add(when: 'CONFIG_SAM460EX', if_true: files('canyonlands.dts'))
dts_ss.add(when: 'CONFIG_PETALOGIX_ML605', if_true: files('petalogix-ml605.dts'))
dts_ss.add(when: 'CONFIG_PETALOGIX_S3ADSP1800', if_true: files('petalogix-s3adsp1800.dts'))

foreach f : dts_ss.all_sources()
  out = fs.replace_suffix(fs.name(f.full_path()), '.dtb')
  custom_target(out,
      build_by_default: have_system,
      input: f,
      output: out,
      install: get_option('install_blobs'),
      install_dir: qemu_datadir / 'dtb',
      command: [ dtc, '-q', '-I', 'dts', '-O', 'dtb',
                      '-o', '@OUTPUT@', '@INPUT0@' ])
endforeach
