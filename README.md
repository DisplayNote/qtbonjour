
QtBonjour
================

This is a fork from an original code from https://github.com/sikoragmbh/qtbonjour


Building
--------

Qt 6.8 LTS is the minimum (`QT_DISABLE_DEPRECATED_BEFORE=0x060800`). On Windows the
Bonjour SDK headers come from Conan (`bonjoursdk/3.0.0@dn/stable`, see `conaninstall.pri`),
so a Conan 1.x client with the `dn` remote and the `msvc19.x86_64.*` profiles is needed;
no local Bonjour SDK install is required.

```
qmake qtbonjour.pro -spec win32-msvc PREFIX=<out>/Windows/Debug CONFIG+=debug
nmake install
```

CI and releases
---------------

`ci/azure-pipelines.yml` builds with [qt-conan-ci](https://github.com/DisplayNote/qt-conan-ci)
2.4.0 (Qt 6.8.8, `msvc2022_64`). Only Windows x86_64 is built, as Montage consumes
qtbonjour on Windows only.

- Pull requests publish `qtbonjour/pr-<number>@dn/develop`.
- A SemVer tag (e.g. `1.3.0`) builds `qtbonjour/<tag>`; the `qtbonjour` release pipeline
  under `libs` then promotes it to `dn/develop` and `dn/stable`.

Qt 6.5 and 6.8 are not ABI compatible: packages built against Qt 6.8 must use a new
version, never a rebuild of an existing one (1.2.0 and earlier predate Qt 6.8).
