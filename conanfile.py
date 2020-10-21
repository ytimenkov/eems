from conans import CMake, CMakeToolchain, ConanFile, tools


class EemsConan(ConanFile):
    name = "eems"
    license = "GPLv3"

    generators = ("cmake_find_package",)
    settings = "os", "arch", "compiler", "build_type"

    scm = {"type": "git", "url": "auto", "revision": "auto"}

    requires = ["boost/1.74.0", "fmt/7.0.3", "pugixml/1.10", "spdlog/1.8.0"]

    def configure(self):
        tools.check_min_cppstd(self, "20")

    def toolchain(self):
        tc = CMakeToolchain(self)
        tc.write_toolchain_files()

    def build(self):
        cmake = CMake(self)

        cmake.configure()
        cmake.build()

        if tools.get_env("CONAN_RUN_TESTS", True):
            with tools.run_environment(self):
                cmake.test(output_on_failure=True)
