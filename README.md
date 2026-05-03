# ViewManagerQt

Standalone Qt widget layout manager used to arrange application views as dock
widgets, tab groups, and split tab panes.

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

## License

MIT. See [LICENSE](LICENSE).
