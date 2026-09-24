## Conan dependencies ##
#
# Windows has no system dns_sd, so the Bonjour SDK headers come from the dn
# Conan remote (bonjoursdk) instead of a local "C:\Program Files\Bonjour SDK"
# install, which the CI agents do not have. macOS, iOS and Linux use the
# dns_sd shipped with the platform and need nothing from here.
#
# The profile is msvc19.x86_64 even though CI builds with VS2022: qt-conan-ci
# names its Conan profiles after the profile set, not the toolchain, and this
# must match conanProfiles in ci/azure-pipelines.yml.
win32 {
    CONFIG -= debug_and_release
    CONFIG += conan_basic_setup

    CONFIG(debug, debug|release) {
        _CONAN_PROFILE_ = msvc19.x86_64.debug
    } else {
        _CONAN_PROFILE_ = msvc19.x86_64.release
    }

    CONAN_COMMAND = conan install $$shell_quote($$PWD) -pr $$_CONAN_PROFILE_ -if $$shell_quote($$PWD) -u --no-imports
    message($$CONAN_COMMAND)

    !system($$CONAN_COMMAND) {
        error("Error processing conan dependencies!!")
    }

    include($$PWD/conanbuildinfo.pri)
}
