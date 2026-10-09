# Debugging with AI — log sanitisation (control 4.7)

> Generated from the `log-sanitise` template (displaynote-engineering plugin) by
> `docs-update`. The "Mandatory step" and "Rules" sections are kept verbatim.

## Where this repo's logs live

| Source | Location / command | Typical sensitive content |
|---|---|---|
| Application log | No log file is written by this repo (no file logger, no `QStandardPaths` log location, no message handler, no logging category, no logging macro). The library logs a single fixed-text `qWarning` (`bonjourserviceregister.cpp:89`, a second register on the same object); everything else it learns — service names, types, domains, TXT records, resolved host names and ports, `DNSServiceErrorType` codes — is only handed to the caller — through signals and through the accessors
`BonjourServiceBrowser::currentRecords()` (`bonjourservicebrowser.h:54-56`) and
`BonjourServiceRegister::registeredRecord()` (`bonjourserviceregister.h:56`) — and the
consuming app decides what to log. Both go through the host process's Qt message handler: in Montage (Windows) that is Montage's handler, so anything logged about Bonjour services ends up in Montage's own log files; without a handler, Qt's default writes to stderr, or to the debugger output when the process has no console. The demo app (`main.cpp`, `qtbonjour_app.pro`) logs with plain `qDebug`: the registered service name and type (line 41) and the DNS-SD error code (line 46) | Library: none (fixed text). Consumer logs and the demo app: service names and host names, which carry device names (free text, often a person's name) and, in the demo's `_raop._tcp` name, the interface MAC address without separators; TXT values (device ids, MACs, model names) |
| Live stream | Windows only: the process's stderr or the debugger output (run it from Qt Creator / Visual Studio, or capture it with DebugView). No `adb logcat`, no journal and no syslog: the repo has no Android, Linux-service or `android.util.Log` / `__android_log_*` code, and CI builds Windows only | the same as the application log row |
| Crash reports | No crash reporter (Sentry or similar) is wired into this repo. Crashes inside the library surface through the consuming app's crash handling (Montage's, when it runs there) | paths, memory addresses |
| CI logs | Azure Pipelines run logs of the `qtbonjour` pipeline (`ci/azure-pipelines.yml`, qt-conan-ci 2.4.0 templates) — the checks reported on `master` are `qtbonjour`, `VERSION ReadVersion`, `BUILD \| windows_x86_64` and `DEPLOY \| develop`; there are no GitHub Actions workflows. The build log includes qmake's echo of the `conan install` command with the agent's absolute source path (`conaninstall.pri:22`) and Conan's own output | internal paths, agent hostnames, Conan remote and package names |
| Customer bundles | Zendesk ticket attachments — download them to a file and sanitise before reading; the Zendesk MCP bypasses the Claude Code guard, so never read attachments through it (see `.agents/skills/log-sanitise/SKILL.md`) | **Protected**: end-user identifiers (1.3 §4.2) |

## Mandatory step — sanitise before any AI tool sees the log

Before a log file, a log excerpt or a live log stream reaches Claude Code, Codex,
Copilot, Cursor, ChatGPT, Claude (Cowork) or any custom OpenAI-API integration,
run it through `dn_logscrub`:

```bash
# Work OUTSIDE the repository: a raw log inside it blocks the searches that would read it
mkdir -p ~/dn-tickets/22416 && cd ~/dn-tickets/22416

# Claude Code (plugin installed) — one call, all files, shared placeholders
/log-sanitise montage.log launcher.log --map ticket-22416.dnmap

# Any agent / shell — the portable copy synced into the repo
python3 <repo>/.agents/skills/log-sanitise/scripts/dn_logscrub.py montage.log launcher.log --map ticket-22416.dnmap

# Inputs in a read-only folder, or elsewhere: write the copies into one directory
python3 <repo>/.agents/skills/log-sanitise/scripts/dn_logscrub.py /var/log/omni/*.log --out-dir ~/dn-tickets/22416

# Live streams — pipe, never paste; use a command that ends (`-d`), not a live tail
adb logcat -d | python3 <repo>/.agents/skills/log-sanitise/scripts/dn_logscrub.py - > logcat.scrubbed.txt

# Customer logs — add the customer's domain(s) so their hostnames are pseudonymised too
python3 <repo>/.agents/skills/log-sanitise/scripts/dn_logscrub.py bundle/*.log --domain acme-school.org
```

A run that is cut short (Ctrl-C, a tool timeout) still writes a trailer marked
`interrupted`: the copy is clean but incomplete. The sanitiser processes a few
MB per second; run very large bundles from your own terminal.

The tool writes `<name>.scrubbed.<ext>` next to each input, prints one summary
line per file (`dn_logscrub: montage.log: 41 redactions (EMAIL=3, ID=12, …)`) and
starts every output with a `# dn_logscrub v…` marker header and ends it with a
`# dn_logscrub end | redactions=…` trailer carrying the per-category counts
(output is written as it is produced, so a live stream appears immediately
instead of waiting for the end). Only files that start with that header and
end with that trailer may be opened by, pasted into, or attached to an AI tool.
A file with the header but no trailer as its last non-empty line is raw:
something was appended after scrubbing (`cat raw >> x.scrubbed.log`,
concatenated files, a process still writing). A trailer ending in
`| interrupted` is accepted — the copy is clean, only incomplete. A
`# dn_logscrub-partial` header does not count: it means rules were switched off
for that run.

## Rules

1. Only `*.scrubbed.*` files (marker header on the first line and the
   `# dn_logscrub end` trailer as the last non-empty line) go to an AI tool.
   Raw logs never do — the Claude Code hook blocks them; for other tools the
   rule is on you.
2. Placeholders are stable within a run: `<EMAIL_1>` is the same person in every
   file of that run. Keep placeholders in PR descriptions, ticket comments and
   Slack messages; never expand them there.
3. The `--map` file (`*.dnmap`) holds the originals for your own reverse lookup.
   It stays on your machine; never attach it, commit it or paste from it.
4. Customer logs are **Protected** data by default (AI Governance Policy 1.3 §4.2).
   Sanitised customer logs may go to Green-List tools with a corporate account.
   Raw customer logs may go to an AI tool only with AI Lead + CEO approval
   (1.3 §4.1) — signal an approved exception with `DN_LOGSCRUB_ALLOW_RAW=1`.
5. Residual check: skim the scrubbed file for anything the patterns missed
   (a person's name in free text, a customer hostname without `--domain`, an
   unusual token format). Fix with `--domain`, or open an issue on
   `displaynote-engineering` with the *category and shape* of the miss — never
   with the value itself.
6. Automated flows (Sentry fixer, Release Management Agent, n8n) call the same
   library (`Scrubber().scrub_text(...)`) before every external model call. A
   flow that writes its result to a file uses `Scrubber().finalize(text, name)`,
   which adds the header and trailer the Claude Code hook and `--check` require.

## What is redacted vs. kept

Redacted irreversibly: private keys, JWTs, auth headers, URL credentials,
cloud/API keys, any `password= / token= / secret= / *_key=` value, OAuth
`code=`/`state=`/`nonce=` in URLs.
Pseudonymised consistently: e-mails, quoted/keyed names (people, devices,
computers), keyed ids (serial, deviceId, session, meetingId, roomPin, tenantId,
licence…), Android `getprop` serials, Wi-Fi SSIDs, IPv4/IPv6, MACs, UUIDs, phone
numbers, the user-home part of any path (Windows with either slash, JSON-escaped,
`file:///`, WSL), Windows `DOMAIN\user` accounts and UNC servers, `DESKTOP-…`
machine names, `.local/.lan/.internal` hosts, `--domain` domains, long hex and
high-entropy strings.
Kept: timestamps, version numbers, stack traces, package/class/method names and
JNI symbols, the rest of the path, file hashes and git commit ids, loopback
addresses, and keys that only describe a secret (`token_expires_in=3600`).
Not caught: a person's name in free text, a customer hostname without `--domain`.

Repo-specific note: the library itself logs nothing sensitive — its only log
line is the fixed-text `qWarning` in `bonjourserviceregister.cpp` (line 89),
which is emitted by release builds too (`QT_NO_DEBUG`/`NDEBUG` do not remove
`qWarning`, and neither `qtbonjour.pro` nor `qtbonjour_app.pro` defines
`QT_NO_WARNING_OUTPUT` or `QT_NO_DEBUG_OUTPUT`). `bonjourserviceresolver.cpp`
includes `<QDebug>` (line 37) but has no log call. The residual risk is in what consumers log from the signals and those accessors: `BonjourRecord` (`serviceName`,
`registeredType`, `replyDomain`, `txtRecord`), the resolved host name and port
(`bonjourRecordResolved`), and error codes. Service and host names are often
device names chosen by a user (frequently a person's name, which the default
rules do not catch in free text) and TXT records can carry device ids and hardware addresses — check them
in the residual pass. The demo app (`main.cpp`, built only from
`qtbonjour_app.pro`, not by CI and not shipped) prints the service name with
`qDebug` (line 41) in every build, since `QT_NO_DEBUG_OUTPUT` is not defined;
for `_raop._tcp` that name is the interface MAC address with the colons removed
plus `@Playmate69` (lines 55-58), and `dn_logscrub` only recognises MAC
addresses written with `:` or `-` separators, so that MAC is not pseudonymised.
Outside the code, every qmake run prints the full `conan install` command with
the absolute source path (`conaninstall.pri:22`), locally (it can contain the
user's home directory) and in the CI build log; nothing in the repo writes
transcripts or log files. No repo-specific sanitiser patterns have been shipped
yet; raise any needed as an issue on `displaynote-engineering` and list them
here once shipped.
