# CLAUDE.md — qtbonjour

@AGENTS.md

`AGENTS.md` (imported above) is the source of truth, including the mandatory
`## Security` section. This file only adds the rules that hurt most when broken.

## DO

- Keep changes small: this is a vendored fork consumed by Montage as a Conan package.
- Keep the shared library `SOURCES`/`HEADERS` in sync between `qtbonjour.pro` and
  `qtbonjour_app.pro` (only the app project lists `main.cpp`; never add it to the library).
- Copy every new `BonjourRecord` member in both the copy constructor and `operator=`.
- Report errors through the existing `error(...)` signals with `DNSServiceErrorType`.
- Keep the Conan profile names in `conaninstall.pri` and `ci/azure-pipelines.yml` equal.
- Release a Qt 6.8 build under a new version (Qt 6.5/6.8 ABI break — see README).
- Before debugging any log, run `/log-sanitise <file…>` and read only the
  `*.scrubbed.*` copy ([docs/runbooks/debugging-with-ai.md](docs/runbooks/debugging-with-ai.md)).

## DON'T

- Don't log service names, host names, TXT values or anything from a `BonjourRecord`.
- Don't edit `.agents/skills/` or `.github/skills/` — they are synced from the
  `displaynote-engineering` plugin (`/sync-skill log-sanitise`).
- Don't run qmake/Conan unless asked: qmake triggers `conan install` against the `dn`
  remote (`conaninstall.pri`).
- Don't assume non-Windows builds work: CI builds Windows x86_64 only.

## Pointers

- Build: [docs/runbooks/local-setup.md](docs/runbooks/local-setup.md)
- Design, threading, platforms: [docs/architecture.md](docs/architecture.md)
- Release flow: [README.md](README.md#ci-and-releases)
