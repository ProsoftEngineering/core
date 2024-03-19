from conans import ConanFile

class OSSCoreTestsConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake_find_package"

    def requirements(self):
        self.requires("catch2/3.5.3")
        self.requires("nlohmann_json/3.9.1")
