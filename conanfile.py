import os
import warnings
from conans import ConanFile
from conans import tools
from conans.errors import ConanException

warnings.filterwarnings('ignore')

class QtbonjourConan(ConanFile):
    #name = 'qtbonjour'
    #version = '1.1.0'
    settings = 'os', 'compiler', 'build_type', 'arch'
    description = '<Description of Qtbonjour here>'
    url = 'http://www.displaynote.com/'
    license = 'None'
    generators = 'qmake'

    def requirements(self):
        # Windows has no system dns_sd; qtbonjour.pro builds against these
        # headers and consumers link dnssd64 from the same package.
        if self.settings.os == 'Windows':
            self.requires('bonjoursdk/3.0.0@dn/stable')

    def package(self):
        # qt-conan-ci installs each build to <os>/<BuildType>/{include,lib}
        # (qmake PREFIX) and ships that tree as the build-folder artifact.
        src = os.path.join(str(self.settings.os), str(self.settings.build_type))
        if not os.path.isdir(os.path.join(self.build_folder, src)):
            # copy() from a missing folder succeeds and exports an empty package.
            raise ConanException("No build output at '{}'".format(src))

        self.copy('*', src=src)

    def package_info(self):
        ## Add libraries stored in lib folder
        self.cpp_info.libs = tools.collect_libs(self)
