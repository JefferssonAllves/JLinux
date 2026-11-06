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
compositor = executable('jlinux-compositor', 'src/compositor/main.c', 'src/compositor/interaction.c', 'src/renderer/renderer.c', 'src/renderer/window-frame.c', 'src/effects/glass.c', 'src/toolkit/painter.c', layer_header, xdg_header,
  dependencies: [wlroots, wayland, xkb, cairo, pango, fontconfig, math], c_args: ['-DWLR_USE_UNSTABLE'], install: true)
frame_test = executable('jlinux-test-window-frame', 'tests/window-frame.c',
  'src/renderer/window-frame.c', 'src/effects/glass.c', 'src/toolkit/painter.c',
  dependencies: [wlroots, cairo, pango, fontconfig, math], c_args: ['-DWLR_USE_UNSTABLE'])
test('window-frame-lifecycle', frame_test)
if get_option('browser')
  wpe = dependency('wpe-webkit-2.0', version: '>=2.54')
  wpe_wayland = dependency('wpe-platform-wayland-2.0')
  wpe_headless = dependency('wpe-platform-headless-2.0')
  browser_resources = import('gnome').compile_resources('browser-resources',
    'data/browser/browser.gresource.xml', source_dir: 'data/browser')
  browser = executable('jlinux-browser', 'src/browser/main.c', 'src/browser/address.c',
    'src/browser/chrome.c', 'src/browser/blocker.c', browser_resources, 'src/toolkit/painter.c', 'src/effects/glass.c',
    c_args: ['-Wno-overlength-strings'], # GLib's generated resource is a binary string.
    dependencies: [wpe, wpe_wayland, wpe_headless, dependency('wayland-client'), dependency('wayland-cursor'),
      xkb, cairo, pango, fontconfig, math, dependency('egl'), dependency('gstreamer-1.0')], install: true)
  address_test = executable('jlinux-test-web-address', 'tests/web-address.c',
    'src/browser/address.c', dependencies: dependency('glib-2.0'))
  test('web-address', address_test)
  test('web-blocklist-data', find_program('tests/browser-blocklist.py'))
  test('web-browser-blocker', find_program('scripts/smoke-web-blocker.py'), args: [browser], timeout: 60)
  test('web-engine-headless', find_program('scripts/smoke-web-engine.py'), args: [browser], timeout: 60)
  install_data('data/browser/COPYING.AdAway', install_dir: get_option('datadir') / 'licenses/jlinux-browser-git')
  install_data('data/browser/README.md', install_dir: get_option('datadir') / 'licenses/jlinux-browser-git', rename: 'AdAway-attribution.md')
  install_data('data/applications/org.jlinux.Browser.desktop', install_dir: get_option('datadir') / 'applications')
endif
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
  greeter = executable('jlinux-greeter', 'src/greeter/main.c', 'src/greeter/auth.c',
    'src/toolkit/painter.c', 'src/effects/glass.c', 'src/renderer/blur.c', client_xdg_header, client_xdg_code,
    dependencies: [client, cursor, cairo, pango, fontconfig, math, xkb, json], install: true)
  auth_test = executable('jlinux-test-login-auth', 'tests/login-auth.c', 'src/greeter/auth.c', dependencies: json)
  test('login-auth', find_program('scripts/smoke-login-auth.py'), args: [auth_test], timeout: 20)
  test('login-headless', find_program('scripts/smoke-login.py'), args: [compositor, greeter], timeout: 20)
  install_data('packaging/greetd/config.toml', install_dir: get_option('datadir') / 'jlinux', rename: 'greetd.toml')
  install_data('packaging/arch/enable-login.sh', install_dir: get_option('libexecdir') / 'jlinux', install_mode: 'rwxr-xr-x')
  install_data('data/wayland-sessions/jlinux.desktop', install_dir: get_option('datadir') / 'wayland-sessions')
  shell = executable('jlinux-shell', 'src/shell/main.c', 'src/toolkit/painter.c', 'src/effects/glass.c', 'src/renderer/blur.c', client_layer_header, client_layer_code, client_xdg_code,
    dependencies: [client, cursor, cairo, pango, fontconfig, math], install: true)
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
  if get_option('browser')
    test('web-browser-wayland', find_program('scripts/smoke-web-browser.py'), args: [compositor, browser, wtype, pointer_test], timeout: 60)
    test('web-browser-media', find_program('scripts/smoke-web-media.py'), args: [compositor, browser, wtype], timeout: 90)
  endif
  if get_option('shell')
    test('login-flow-headless', find_program('scripts/smoke-login-flow.py'), args: [compositor, greeter, wtype], timeout: 25)
    test('shell-headless', shell_smoke, args: [compositor, shell, pointer_test], timeout: 20)
  endif
  test('window-frame-input', find_program('scripts/smoke-window-frame.py'), args: [compositor, pointer_test, foot, wtype], timeout: 35)
  terminal_test = find_program('scripts/smoke-terminal.py')
  test('terminal-input-headless', terminal_test, args: [compositor, pointer_test, foot, wtype], timeout: 40)
else
  warning('Teste de terminal/entrada indisponível: instale foot e wtype')
  if get_option('shell')
    test('login-flow-headless', find_program('scripts/smoke-login-flow.py'), args: [compositor, greeter, wtype], timeout: 25)
    test('shell-headless', shell_smoke, args: [compositor, shell], timeout: 15)
  endif
endif
