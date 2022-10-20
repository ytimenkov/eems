from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, cmake_layout

from conan import ConanFile


class EemsConan(ConanFile):
    name = "eems"

    generators = "CMakeDeps", "CMakeToolchain"
    settings = "os", "arch", "compiler", "build_type"

    options = {"shared": [True, False], "fPIC": [True, False]}

    default_options = {
        "shared": False,
        "fPIC": True,
        "date:use_system_tz_db": True,
        "boost:header_only": False,
        "boost:system_no_deprecated": True,
        "boost:asio_no_deprecated": True,
        "boost:filesystem_no_deprecated": True,
    }

    requires = [
        "boost/1.80.0",
        "fmt/9.1.0",
        "pugixml/1.12.1",
        "spdlog/1.10.0",
        "range-v3/0.12.0",
        "toml11/3.7.1",
        "leveldb/1.23",
        "flatbuffers/2.0.8",
        "date/3.0.1",
    ]

    tool_requires = [
        "flatbuffers/2.0.8",
    ]

    def validate(self):
        check_min_cppstd(self, "20")

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)

        cmake.configure()
        cmake.build()
        cmake.test(output_on_failure=True)
