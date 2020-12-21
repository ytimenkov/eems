from conans import CMake, CMakeToolchain, ConanFile, tools


class EemsConan(ConanFile):
    name = "eems"

    generators = ("cmake_find_package",)
    settings = "os", "arch", "compiler", "build_type"

    scm = {"type": "git", "url": "auto", "revision": "auto"}

    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    requires = [
        "boost/1.75.0",
        "fmt/7.1.3",
        "pugixml/1.10",
        "spdlog/1.8.2",
        "range-v3/0.11.0",
        "toml11/3.6.0",
        "leveldb/1.22",
        "flatbuffers/1.12.0",
        "flatc/1.12.0",
    ]

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

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
