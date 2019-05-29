import os
import warnings
from conans import ConanFile
from conans import tools

warnings.filterwarnings('ignore')

class QtbonjourConan(ConanFile):
    #name = 'qtbonjour'
    #version = '1.1.0'
    settings = 'os', 'compiler', 'build_type', 'arch'
    description = '<Description of Qtbonjour here>'
    url = 'http://www.displaynote.com/'
    license = 'copyright'
    license = 'None'

    def package(self):
        self.copy('*', 'include', os.path.join('install', str(self.settings.os), str(self.settings.build_type), 'include'))
        self.copy('*', 'lib', os.path.join('install', str(self.settings.os), str(self.settings.build_type), 'lib'))

    def package_info(self):
        ## Add libraries stored in lib folder
        self.cpp_info.libs = tools.collect_libs(self)
