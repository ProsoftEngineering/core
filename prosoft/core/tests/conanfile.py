from conans import ConanFile

class OSSCoreTestsConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake_find_package"

    def requirements(self):
        self.requires("catch2/2.13.3")
