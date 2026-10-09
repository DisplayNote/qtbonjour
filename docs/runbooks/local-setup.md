# Local setup

Windows x86_64 with MSVC is the only build CI runs (see
[../architecture.md](../architecture.md#platform-support)); the steps below follow it.
[README.md](../../README.md#building) has the short version.

## 1. Clone

```bash
git clone git@github.com:DisplayNote/qtbonjour.git
```

Run `/secret-scan-setup` once per clone (or `--global` once per machine for new clones) — commits containing secrets are blocked locally and in CI.
`/secret-scan-setup` is a Claude Code command from DisplayNote's `displaynote-engineering` plugin (install it with `/plugin install displaynote-engineering`); it is not part of this repository.

## 2. Prerequisites (Windows)

- Visual Studio 2022 C++ toolset (CI uses the `msvc2022_64` Qt kit).
- Qt 6.8 or later for `msvc2022_64` (CI: Qt 6.8.8 LTS). The `.pro` files set
  `QT_DISABLE_DEPRECATED_BEFORE=0x060800`, so older Qt versions are not supported.
- A Conan **1.x** client (`conanfile.py` imports `conans`, the Conan 1 API) configured
  with DisplayNote's `dn` remote and the `msvc19.x86_64.debug` / `msvc19.x86_64.release`
  profiles (the names `conaninstall.pri` passes to `conan install`).
- The Bonjour service installed and running, only if you want to run the demo app.

No local Bonjour SDK is needed: qmake runs `conan install` (from `conaninstall.pri`) and
gets `bonjoursdk/3.0.0@dn/stable`. That writes `conanbuildinfo.*` / `conaninfo.txt` into
the repository root (gitignored).

## 3. Build and install the library

From the repository root, in an "x64 Native Tools Command Prompt for VS 2022" with Qt's
`bin` directory on `PATH`:

```bat
set "QB_PREFIX=%CD%\install\Windows\Debug"
mkdir build-debug
pushd build-debug && qmake ..\qtbonjour.pro -spec win32-msvc PREFIX="%QB_PREFIX%" CONFIG+=debug && nmake install & popd
```

The library (`qtbonjour.lib`, static on Windows) and the headers land in
`install\Windows\Debug\{lib,include}` — the `<os>/<BuildType>` layout that
`conanfile.py` packages. Use `CONFIG+=release` and a `Release` prefix for a release
build. `build*/` and `install/` are gitignored.

## 4. Optional: demo app

`qtbonjour_app.pro` compiles the library sources together with `main.cpp` into an app
that registers sample `_raop._tcp`, `_airplay._tcp` and `_playmate._tcp` services on every
up, running, multicast-capable, non-loopback, non-point-to-point interface. It needs the Qt Network, Qml,
Quick, Concurrent and Widgets modules (`QT +=` in `qtbonjour_app.pro`). Build it in its
own directory (its target is also called `qtbonjour`):

```bat
mkdir build-app
pushd build-app && qmake ..\qtbonjour_app.pro -spec win32-msvc CONFIG+=debug && nmake & popd
```

The app prints `Service registered` (or `Error registering service` with the DNS-SD error
code) per registration, then about 5 s later deletes all the registrations
(`main.cpp:124-132`), so check the services with a Bonjour browser on another machine
within that window.

## Other platforms

`qtbonjour.pro` also has defaults for Linux (Avahi compat `dns_sd`), macOS and iOS
(system `dns_sd`), but CI does not build them and they are not published. A plain
`qmake qtbonjour.pro && make install` is the expected flow there; it has not been
verified in this repository.

> Commands in this runbook were not executed while writing it (documentation rollout
> rule: no builds or Conan runs); they mirror the README and the build files.
