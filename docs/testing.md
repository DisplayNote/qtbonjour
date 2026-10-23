# Testing

The automated suite lives in `tests/`: GoogleTest + GoogleMock, run by CI with
coverage. Every change to the library gets its test there. `qtbonjour_app.pro`
(`main.cpp`) is a demo app that registers real services; it is not a test.

## What is covered

| File | Covers |
|---|---|
| `tests/tst_bonjourrecord.cpp` | `BonjourRecord`: constructors (QString and UTF-8 `char *`), copy, assignment, `operator==` ignoring the TXT record, meta-type registration |
| `tests/tst_bonjourservicebrowser.cpp` | `BonjourServiceBrowser`: browse start-up and errors, add/remove/de-duplication, `MoreComing` batching, error replies, socket processing, release on destruction |
| `tests/tst_bonjourserviceresolver.cpp` | `BonjourServiceResolver`: resolve call, port byte order, TXT parsing, error paths (rejected resolve, missing socket, error reply, unreadable TXT), release of pending resolves |
| `tests/tst_bonjourserviceregister.cpp` | `BonjourServiceRegister`: register call on its worker thread, port byte swap, TXT building, error emission, final record, self-deletion on unregister |

Some tests pin current behaviour that looks wrong (each says *Current behaviour*
in a comment): a copied record takes the original's parent, a failed resolve is
never released, an unreadable TXT record is reported with `kDNSServiceErr_NoError`,
non-ASCII TXT values lose bytes on register, and a register without TXT entries
reads a `TXTRecordRef` that was never created. Change the test together with the
fix.

## Rules

- Tests never use the Bonjour service. The test executable links
  `tests/fakes/fake_dns_sd.cpp` instead of `dnssd64`: it defines the dns_sd
  functions the library calls, records each call with its reply callback and
  context, and lets a test set the result of each call
  (`tests/fakes/fake_dns_sd.h`). A test plays the daemon by calling the recorded
  callback. Call `fakedns::reset()` in `SetUp`.
- The fake's `TXTRecord*` functions use the SDK's wire format (length-prefixed
  `key=value` strings); build expected bytes with `fakedns::txtRecord()`.
- `BonjourServiceRegister` moves itself to its own `QThread`, so its signals come
  from that thread. Use the `RegisterRun` helper in
  `tests/tst_bonjourserviceregister.cpp`: it records signals under a lock, and
  `stop()` waits until the object has deleted itself.
- `main()` comes from googletest (`gmock_main`). The `QCoreApplication` that socket
  notifiers and threads need is created once by the global environment in
  `tests/support/qtapplication.cpp`.
- googletest comes from Conan (`tests/conanfile.py`, `googletest/1.15.2@dn/stable`)
  and is never added to the library's `conanfile.py`. The test conanfile also
  requires `bonjoursdk/3.0.0@dn/stable`, for `dns_sd.h` only; `tests/conan.pri`
  takes its include path but not its library.
- One `tst_<class>.cpp` per class, test names describe behaviour. Add new files to
  `tests/qtbonjourtests.pro`.

## Build and run locally (Windows, Debug)

The test project links the static library built from `qtbonjour.pro` in the
**same build directory**, so build the library there first. From a Visual Studio
x64 prompt with Qt 6.8.8 `msvc2022_64` and Conan 1.x (the `dn` remote and profile
`msvc19.x86_64.debug`):

```bat
mkdir build-tests && cd build-tests
qmake ..\qtbonjour.pro CONFIG+=debug CONFIG-=debug_and_release && jom
qmake ..\tests\qtbonjourtests.pro CONFIG+=debug CONFIG-=debug_and_release && jom
set PATH=<Qt>\6.8.8\msvc2022_64\bin;%PATH%
qtbonjourtests.exe
```

The suite takes well under a second.

## In CI

`ci/azure-pipelines.yml` runs the suite in the `tests_coverage` stage
(`windows-x86_64/coverage.yml@ci`, qt-conan-ci 2.5.0, roadmap 2.23). The stage:

1. builds the library and then the tests in Debug;
2. runs `qtbonjourtests.exe` under OpenCppCoverage;
3. publishes JUnit results and a Cobertura report to the run;
4. checks line coverage against the 75% target.

It is in audit mode (`blocking: false`): failing tests or a figure below target
show as warnings and the build still runs. Set `blocking: true` once the suite has
been green on five consecutive runs. From then on a PR must not lower the figure.

What the figure measures is in the README, under *Coverage scope*.
