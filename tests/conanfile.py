from conans import ConanFile


class QtbonjourTestsConan(ConanFile):
    """Test-only dependencies. Never add these to the library's conanfile.py."""
    settings = "os", "compiler", "build_type", "arch"
    generators = "qmake"

    def requirements(self):
        self.requires("googletest/1.15.2@dn/stable")
        # dns_sd.h only: the tests link tests/fakes/fake_dns_sd.cpp instead of
        # dnssd64 (see conan.pri).
        self.requires("bonjoursdk/3.0.0@dn/stable")
