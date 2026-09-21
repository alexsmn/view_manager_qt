# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with
code in this repository.

`view_manager_qt` is a Qt6 dock/view manager widget, and a product in its own
right under ADR 0011: it builds and tests standalone, and it consumes no other
product. The one thing that is not obvious from the sources is how its tests
get a headless platform, which is what this file is for.

## The tests run offscreen on macOS

`tests/view_manager_qt_test_main.cpp` calls
`scada::qt_test::DefaultToOffscreenPlatform()` before it builds the
`QApplication`, which on macOS sets `QT_QPA_PLATFORM` to
`offscreen:configfile=<a 1920x1080 screen>` unless the caller named a
platform. Without it, a `ctest` sweep — one process per case — bounced a Dock
icon and stole focus once per case.

Two things have to be true for that to work, and neither was until 2026-09-20:

- **The plugin has to be in the Qt.** This is a `vcpkg.json` question, not a
  code one: qtbase gates `src/plugins/platforms/offscreen` on
  `QT_FEATURE_freetype`, so with `"default-features": false` and no
  `freetype` the plugin is simply not built. `$direct` names it, beside Linux
  `fontconfig`, for that reason alone — the same pair, for the same reason,
  as `display`. `QT_QPA_PLATFORM=nosuchplatform` on the test binary prints
  the list; it must say `cocoa, offscreen`.
- **The plugin has to be linked in**, because the Qt here is static.
  `scada_qt_import_offscreen_platform_into_tests()` at the end of
  `CMakeLists.txt` does that, and passes the screen description's path as the
  `SCADA_QT_OFFSCREEN_PLATFORM_CONFIG` definition, which is the only thing
  the helper keys on.

The helper is `build-support/scada_qt_offscreen_platform.h` — shared, not
copied, because this product may include from no other one — and it is
reached through the `include_directories("${SCADA_BUILD_SUPPORT_DIR}")` near
the top of `CMakeLists.txt`.
