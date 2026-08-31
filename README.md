# ViewManagerQt

Standalone Qt widget layout manager used to arrange application views as dock
widgets, tab groups, and split tab panes.

## Screenshots

![Component workflow](testdata/component_workflow.png)
![Open layout](testdata/open_layout.png)

## CMake

Add this directory to `CMAKE_MODULE_PATH`, then use:

```cmake
find_package(ViewManagerQt REQUIRED)

target_link_libraries(my_target PRIVATE
  view_manager_qt
)
```

The package defines:

- `view_manager_qt`
- `ViewManagerQt::view_manager_qt`

## Requirements

- CMake 3.25 or newer
- C++20
- Qt5 Widgets

## Updating Golden Screenshots

The integration tests compare rendered widget output against golden images in
`testdata/`. To update screenshots after an intentional visual change:

1. Delete the outdated image from `testdata/`
2. Run `view_manager_qt_unittests`
3. Verify the generated image
4. Commit the updated image

When comparison fails, the actual output is saved as `testdata/actual_*.png`.

Deleting the image is the *only* way to ask for a regenerated golden. A file
that is present but cannot be decoded — a truncated PNG, or any PNG at all in a
build without the codec — fails the test instead, because regenerating there
would silently replace a reviewed baseline with whatever the current code
renders. Restore such a file from git rather than deleting it.

**On macOS the golden comparison is recorded, not asserted**, because the
rendering is platform-specific. It deliberately does *not* `GTEST_SKIP`: this
helper runs at the *end* of cases that have already asserted a great deal, and
skipping aborts the whole case. While it did, a case whose assertions passed
and a case whose assertions failed both reported `SKIPPED` — an outcome that is
the same either way is one nobody reads, and it hid a real failure in
`PublicApiWorkflow` for two weeks while also masking `OpenLayoutRestoresSavedShape`
entirely. A mismatch is now logged and the case reports what its own assertions
did.

## Window activation and the focus-derived assertions

`ViewManagerQtComponent::GetActiveViewId()` is defined as
`FindViewIdByWidget(QApplication::focusObject())`, and the active-view handler
is driven by the same object through `focusObjectChanged`. `focusObject()` is
the focus object of the application's **active window**, so both are null
whenever this process does not own activation — and `activateWindow()` is a
request the window server is free to refuse.

**A test process essentially never owns activation**: any focused application
defeats it, and on a headless or CI run there is no active window at all. This
is machine-wide state no test can control, so assertions that read
`focusObject()` are gated on `main_window.isActiveWindow()` and log when they
are skipped.

`RUN_SERIAL` on the isolated test (see `CMakeLists.txt`) narrows the window and
cannot close it: ctest can stop a *sibling test* from contending, and nothing
there can stop a foreground application that is not a test. Do not treat it as
the fix.

Nothing goes uncovered by the gate. `ActivateView`'s observable effects do not
depend on activation and are asserted unconditionally by
`ViewManagerQtComponentTest.ActivateViewSelectsTheTabOfANonDockedView` and
`.ActivateViewRaisesATabifiedDock`.

## License

MIT. See [LICENSE](LICENSE).
