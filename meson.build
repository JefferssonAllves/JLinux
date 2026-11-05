project('jlinux-desktop', 'c', version: '0.2.0',
  default_options: ['c_std=c11', 'warning_level=3', 'werror=true'])
wlroots = dependency('wlroots-0.20', version: ['>=0.20.0', '<0.21.0'])
wayland = dependency('wayland-server')
executable('jlinux-compositor', 'src/compositor/main.c',
  dependencies: [wlroots, wayland], c_args: ['-DWLR_USE_UNSTABLE'], install: true)
if get_option('panel')
  gtk = dependency('gtk+-3.0', version: '>=3.22')
  layer_shell = dependency('gtk-layer-shell-0', version: '>=0.6')
  executable('jlinux-panel', 'src/panel.c', dependencies: [gtk, layer_shell], install: true)
endif
smoke = find_program('scripts/smoke-compositor.sh')
test('compositor-headless', smoke, args: [meson.current_build_dir() / 'jlinux-compositor'], timeout: 15)
