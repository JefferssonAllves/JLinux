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
math = meson.get_compiler('c').find_library('m')
cairo = dependency('cairo')
pango = dependency('pangocairo')
fontconfig = dependency('fontconfig')
compositor = executable('jlinux-compositor', 'src/compositor/main.c', 'src/compositor/interaction.c', 'src/shell/control.c', 'src/renderer/renderer.c', 'src/renderer/window-frame.c', 'src/renderer/window-effects.c', 'src/effects/window.c', 'src/animation/transition.c', 'src/effects/glass.c', 'src/toolkit/painter.c', layer_header, xdg_header,
  dependencies: [wlroots, wayland, xkb, cairo, pango, fontconfig, math], c_args: ['-DWLR_USE_UNSTABLE'], install: true)
frame_test = executable('jlinux-test-window-frame', 'tests/window-frame.c',
  'src/renderer/window-frame.c', 'src/effects/glass.c', 'src/toolkit/painter.c',
  dependencies: [wlroots, cairo, pango, fontconfig, math], c_args: ['-DWLR_USE_UNSTABLE'])
test('window-frame-lifecycle', frame_test)
window_effects_test = executable('jlinux-test-window-effects', 'tests/window-effects.c',
  'src/renderer/window-effects.c', 'src/effects/window.c', 'src/renderer/window-frame.c', 'src/effects/glass.c', 'src/toolkit/painter.c',
  dependencies: [wlroots, wayland, cairo, pango, fontconfig, math], c_args: ['-DWLR_USE_UNSTABLE'])
test('window-effects-lifecycle', window_effects_test)
panel_test = executable('jlinux-test-glass-panel', 'tests/glass-panel.c', 'src/effects/glass.c', 'src/toolkit/painter.c',
  dependencies: [cairo, pango, fontconfig, math])
test('glass-panel-fade', panel_test)
transition_test = executable('jlinux-test-transition', 'tests/transition.c', 'src/animation/transition.c', dependencies: math)
test('animation-transition', transition_test)
battery_test = executable('jlinux-test-battery', 'tests/battery.c', 'src/shell/battery.c')
test('battery-status', battery_test)
dock_visual_test = executable('jlinux-test-dock-visual', 'tests/dock-visual.c', 'src/renderer/dock-transform.c', 'src/effects/dock.c', dependencies: [cairo, math])
test('dock-visual', dock_visual_test)
if get_option('shell')
  client = dependency('wayland-client')
  cursor = dependency('wayland-cursor')
  client_layer_header = custom_target('layer-client-header',
    input: 'protocols/wlr-layer-shell-unstable-v1.xml', output: 'layer-shell-client.h',
    command: [scanner, 'client-header', '@INPUT@', '@OUTPUT@'])
  client_layer_code = custom_target('layer-client-code',
    input: 'protocols/wlr-layer-shell-unstable-v1.xml', output: 'layer-shell-client.c',
    command: [scanner, 'private-code', '@INPUT@', '@OUTPUT@'])
  client_xdg_code = custom_target('xdg-client-code',
    input: protocol_dir / 'stable/xdg-shell/xdg-shell.xml', output: 'xdg-shell-client.c',
    command: [scanner, 'private-code', '@INPUT@', '@OUTPUT@'])
  json = dependency('json-c')
  client_xdg_header = custom_target('xdg-client-header',
    input: protocol_dir / 'stable/xdg-shell/xdg-shell.xml', output: 'xdg-shell-client.h',
    command: [scanner, 'client-header', '@INPUT@', '@OUTPUT@'])
  glib = dependency('glib-2.0')
  files = executable('jlinux-files', 'src/files/main.c', 'src/files/browser.c', 'src/session/locale.c',
    'src/toolkit/painter.c', 'src/effects/glass.c', client_xdg_header, client_xdg_code,
    dependencies: [client, cursor, cairo, pango, fontconfig, math, xkb, glib], install: true)
  browser_test = executable('jlinux-test-browser', 'tests/browser.c', 'src/files/browser.c', dependencies: glib)
  test('files-browser', find_program('scripts/smoke-browser.py'), args: [browser_test], timeout: 15)
  install_data('data/xdg/mimeapps.list', install_dir: get_option('sysconfdir') / 'xdg')
  install_data('data/applications/org.jlinux.Files.desktop', install_dir: get_option('datadir') / 'applications')
  greeter = executable('jlinux-greeter', 'src/greeter/main.c', 'src/greeter/auth.c',
    'src/toolkit/painter.c', 'src/effects/glass.c', 'src/renderer/blur.c', client_xdg_header, client_xdg_code,
    dependencies: [client, cursor, cairo, pango, fontconfig, math, xkb, json], install: true)
  auth_test = executable('jlinux-test-login-auth', 'tests/login-auth.c', 'src/greeter/auth.c', dependencies: json)
  test('login-auth', find_program('scripts/smoke-login-auth.py'), args: [auth_test], timeout: 20)
  test('login-headless', find_program('scripts/smoke-login.py'), args: [compositor, greeter], timeout: 20)
  install_data('packaging/greetd/config.toml', install_dir: get_option('datadir') / 'jlinux', rename: 'greetd.toml')
  install_data('packaging/arch/enable-login.sh', install_dir: get_option('libexecdir') / 'jlinux', install_mode: 'rwxr-xr-x')
  install_data('data/wayland-sessions/jlinux.desktop', install_dir: get_option('datadir') / 'wayland-sessions')
  install_data('data/futurista/README.md', install_dir: get_option('datadir') / 'licenses/jlinux-desktop-git', rename: 'futurista-assets.md')
  futurista_resources = import('gnome').compile_resources('futurista-resources', 'data/futurista/assets.xml', source_dir: 'data/futurista')
  shell = executable('jlinux-shell', 'src/shell/main.c', 'src/shell/battery.c', 'src/shell/volume.c', 'src/shell/control.c', 'src/shell/futurista.c', 'src/effects/dock.c', 'src/renderer/dock-transform.c', 'src/animation/transition.c', futurista_resources, 'src/toolkit/painter.c', 'src/effects/glass.c', 'src/renderer/blur.c', client_layer_header, client_layer_code, client_xdg_code,
    c_args: ['-Wno-overlength-strings'], dependencies: [client, cursor, cairo, pango, fontconfig, math, dependency('gio-2.0')], install: true)
  volume_test = executable('jlinux-test-volume', 'tests/volume.c', 'src/shell/volume.c', dependencies: [glib, math])
  test('volume-response', volume_test)
  blur_test = executable('jlinux-test-blur', 'tests/blur.c', 'src/renderer/blur.c', dependencies: cairo)
  test('blur-image', blur_test)
  test('glass-preview', find_program('scripts/smoke-glass.py'), args: [shell], timeout: 20)
  session = executable('jlinux-session', 'src/session/main.c', 'src/session/locale.c', install: true)
  locale_test = executable('jlinux-test-session-locale', 'tests/session-locale.c', 'src/session/locale.c')
  test('session-locale', locale_test)
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
    test('files-headless', find_program('scripts/smoke-files.py'), args: [compositor, files, pointer_test, wtype], timeout: 30)
    test('login-flow-headless', find_program('scripts/smoke-login-flow.py'), args: [compositor, greeter, wtype], timeout: 25)
    test('futurista-headless', find_program('scripts/smoke-futurista.py'), args: [compositor, shell, wtype, pointer_test], timeout: 30)
    test('dock-reduced-motion', find_program('scripts/smoke-futurista.py'), args: [compositor, shell, wtype, pointer_test], env: ['JLINUX_REDUCED_MOTION=1'], timeout: 30)
    test('shell-headless', shell_smoke, args: [compositor, shell, pointer_test], timeout: 20)
    test('volume-headless', find_program('scripts/smoke-volume.py'), args: [compositor, shell, pointer_test], timeout: 35)
    test('power-menu-headless', find_program('scripts/smoke-power.py'), args: [compositor, shell, pointer_test], timeout: 35)
  endif
  if get_option('shell')
    layout_client = executable('jlinux-test-layout-client', 'tests/layout-client.c', client_xdg_header, client_xdg_code,
      dependencies: client_dep)
    test('window-transitions', find_program('scripts/smoke-window-transitions.py'), args: [compositor, layout_client], timeout: 20)
    test('window-reduced-motion', find_program('scripts/smoke-window-transitions.py'), args: [compositor, layout_client], env: ['JLINUX_REDUCED_MOTION=1'], timeout: 20)
    test('window-layout-constraints', find_program('scripts/smoke-layout-constraints.py'), args: [compositor, layout_client, wtype, pointer_test], timeout: 25)
  endif
  test('window-side-snap', find_program('scripts/smoke-side-snap.py'), args: [compositor, shell, pointer_test, foot, wtype], timeout: 40)
  test('window-frame-input', find_program('scripts/smoke-window-frame.py'), args: [compositor, pointer_test, foot, wtype], timeout: 35)
  terminal_test = find_program('scripts/smoke-terminal.py')
  test('terminal-input-headless', terminal_test, args: [compositor, pointer_test, foot, wtype], timeout: 40)
else
  warning('Teste de terminal/entrada indisponível: instale foot e wtype')
  if get_option('shell')
    test('files-headless', find_program('scripts/smoke-files.py'), args: [compositor, files, pointer_test, wtype], timeout: 30)
    test('login-flow-headless', find_program('scripts/smoke-login-flow.py'), args: [compositor, greeter, wtype], timeout: 25)
    test('futurista-headless', find_program('scripts/smoke-futurista.py'), args: [compositor, shell, wtype, pointer_test], timeout: 30)
    test('shell-headless', shell_smoke, args: [compositor, shell], timeout: 15)
  endif
endif
