project('jlinux-desktop', 'c', meson_version: '>=1.6.0', version: '0.5.0',
  default_options: ['c_std=c11', 'warning_level=3', 'werror=true'])
wlroots = dependency('wlroots-0.20', version: ['>=0.20.0', '<0.21.0'])
wayland = dependency('wayland-server')
xkb = dependency('xkbcommon')
scanner = find_program('wayland-scanner')
protocols = dependency('wayland-protocols')
protocol_dir = protocols.get_variable(pkgconfig: 'pkgdatadir')
layer_header = custom_target('layer-shell-header',
  input: 'protocols/wlr-layer-shell-unstable-v1.xml', output: 'wlr-layer-shell-unstable-v1-protocol.h',
  command: [scanner, 'server-header', '@INPUT@', '@OUTPUT@'])
xdg_header = custom_target('xdg-shell-header',
  input: protocol_dir / 'stable/xdg-shell/xdg-shell.xml', output: 'xdg-shell-protocol.h',
  command: [scanner, 'server-header', '@INPUT@', '@OUTPUT@'])
compositor = executable('jlinux-compositor', 'src/compositor/main.c', 'src/compositor/interaction.c', 'src/renderer/renderer.c', layer_header, xdg_header,
  dependencies: [wlroots, wayland, xkb], c_args: ['-DWLR_USE_UNSTABLE'], install: true)
if get_option('shell')
  client = dependency('wayland-client')
  cursor = dependency('wayland-cursor')
  cairo = dependency('cairo')
  pango = dependency('pangocairo')
  fontconfig = dependency('fontconfig')
  client_layer_header = custom_target('layer-client-header',
    input: 'protocols/wlr-layer-shell-unstable-v1.xml', output: 'layer-shell-client.h',
    command: [scanner, 'client-header', '@INPUT@', '@OUTPUT@'])
  client_layer_code = custom_target('layer-client-code',
    input: 'protocols/wlr-layer-shell-unstable-v1.xml', output: 'layer-shell-client.c',
    command: [scanner, 'private-code', '@INPUT@', '@OUTPUT@'])
  client_xdg_code = custom_target('xdg-client-code',
    input: protocol_dir / 'stable/xdg-shell/xdg-shell.xml', output: 'xdg-shell-client.c',
    command: [scanner, 'private-code', '@INPUT@', '@OUTPUT@'])
  shell = executable('jlinux-shell', 'src/shell/main.c', 'src/toolkit/painter.c', 'src/effects/glass.c', 'src/renderer/blur.c', client_layer_header, client_layer_code, client_xdg_code,
    dependencies: [client, cursor, cairo, pango, fontconfig], install: true)
  blur_test = executable('jlinux-test-blur', 'tests/blur.c', 'src/renderer/blur.c', dependencies: cairo)
  test('blur-image', blur_test)
  test('glass-preview', find_program('scripts/smoke-glass.py'), args: [shell], timeout: 20)
  session = executable('jlinux-session', 'src/session/main.c', install: true)
  session_smoke = find_program('scripts/smoke-session.py')
  test('session-headless', session_smoke, args: [session], timeout: 20)
  shell_smoke = find_program('scripts/smoke-shell.py')
endif
smoke = find_program('scripts/smoke-compositor.sh')
test('compositor-headless', smoke, args: [meson.current_build_dir() / 'jlinux-compositor'], timeout: 15)
foot = find_program('foot', required: false)
wtype = find_program('wtype', required: false)
if foot.found() and wtype.found()
  client_dep = dependency('wayland-client')
  pointer_header = custom_target('virtual-pointer-header',
    input: 'protocols/wlr-virtual-pointer-unstable-v1.xml', output: 'virtual-pointer-client.h',
    command: [scanner, 'client-header', '@INPUT@', '@OUTPUT@'])
  pointer_code = custom_target('virtual-pointer-code',
    input: 'protocols/wlr-virtual-pointer-unstable-v1.xml', output: 'virtual-pointer-protocol.c',
    command: [scanner, 'private-code', '@INPUT@', '@OUTPUT@'])
  pointer_test = executable('jlinux-test-pointer', 'tests/pointer-client.c', pointer_header, pointer_code,
    dependencies: client_dep)
  if get_option('shell')
    test('shell-headless', shell_smoke, args: [compositor, shell, pointer_test], timeout: 20)
  endif
  terminal_test = find_program('scripts/smoke-terminal.py')
  test('terminal-input-headless', terminal_test, args: [compositor, pointer_test, foot, wtype], timeout: 40)
else
  warning('Teste de terminal/entrada indisponível: instale foot e wtype')
  if get_option('shell')
    test('shell-headless', shell_smoke, args: [compositor, shell], timeout: 15)
  endif
endif
