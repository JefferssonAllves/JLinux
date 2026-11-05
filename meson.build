project('jlinux-shell', 'c', version: '0.1.0',
  default_options: ['c_std=c11', 'warning_level=3', 'werror=true'])
gtk = dependency('gtk+-3.0', version: '>=3.22')
layer_shell = dependency('gtk-layer-shell-0', version: '>=0.6')
executable('jlinux-panel', 'src/panel.c', dependencies: [gtk, layer_shell], install: true)
install_data('config/sway/config', install_dir: get_option('datadir') / 'jlinux')
