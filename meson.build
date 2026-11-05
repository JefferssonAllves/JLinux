project('jlinux-desktop', 'c', version: '0.3.0',
  default_options: ['c_std=c11', 'warning_level=3', 'werror=true'])
wlroots = dependency('wlroots-0.20', version: ['>=0.20.0', '<0.21.0'])
wayland = dependency('wayland-server')
scanner = find_program('wayland-scanner')
protocols = dependency('wayland-protocols')
protocol_dir = protocols.get_variable(pkgconfig: 'pkgdatadir')
layer_header = custom_target('layer-shell-header',
  input: 'protocols/wlr-layer-shell-unstable-v1.xml', output: 'wlr-layer-shell-unstable-v1-protocol.h',
  command: [scanner, 'server-header', '@INPUT@', '@OUTPUT@'])
xdg_header = custom_target('xdg-shell-header',
  input: protocol_dir / 'stable/xdg-shell/xdg-shell.xml', output: 'xdg-shell-protocol.h',
  command: [scanner, 'server-header', '@INPUT@', '@OUTPUT@'])
compositor = executable('jlinux-compositor', 'src/compositor/main.c', layer_header, xdg_header,
  dependencies: [wlroots, wayland], c_args: ['-DWLR_USE_UNSTABLE'], install: true)
if get_option('shell')
  gtk = dependency('gtk+-3.0', version: '>=3.22')
  layer_shell = dependency('gtk-layer-shell-0', version: '>=0.6')
  shell = executable('jlinux-shell', 'src/shell/main.c', dependencies: [gtk, layer_shell], install: true)
  shell_smoke = find_program('scripts/smoke-shell.py')
  test('shell-headless', shell_smoke, args: [compositor, shell], timeout: 15)
endif
smoke = find_program('scripts/smoke-compositor.sh')
test('compositor-headless', smoke, args: [meson.current_build_dir() / 'jlinux-compositor'], timeout: 15)
