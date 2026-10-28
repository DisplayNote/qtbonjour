# googletest and the Bonjour SDK headers from Conan, for the Windows test build
# (docs/testing.md). Installs into the build directory, so the source tree stays
# clean.
#
# No conan_basic_setup on purpose: it would link every package's libraries, and
# dnssd64 must stay out of the test executable. tests/fakes/fake_dns_sd.cpp
# defines the dns_sd functions instead, so the tests never need the Bonjour
# service. Only the include paths and googletest's libraries are taken.
win32 {
    CONFIG(debug, debug|release): _TESTS_CONAN_PROFILE_ = msvc19.x86_64.debug
    else: _TESTS_CONAN_PROFILE_ = msvc19.x86_64.release

    !system(conan install $$shell_quote($$shell_path($$PWD)) -pr $$_TESTS_CONAN_PROFILE_ -if $$shell_quote($$shell_path($$OUT_PWD)) -u) {
        error("conan install failed for the test dependencies")
    }

    include($$OUT_PWD/conanbuildinfo.pri)
    INCLUDEPATH += $$CONAN_INCLUDEPATH
    LIBS += $$CONAN_LIBDIRS_GOOGLETEST $$CONAN_LIBS_GOOGLETEST
} else {
    error("The tests build on Windows only for now (docs/testing.md).")
}
