from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps
import os


class Zonvras(ConanFile):
    name = "zonvras"
    settings = "os", "compiler", "build_type", "arch"
    requires = [
        "glfw/3.3.8",
        "glm/0.9.9.8",
        "vulkan-headers/1.3.243.0",
        "imgui/1.90.4",
    ]
    default_options = {
        "glfw/*:shared": False,
        "imgui/*:shared": False,
    }

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

        # Write the imgui package folder to a CMake include file so CMakeLists
        # can locate the backend source files (imgui_impl_vulkan.cpp, etc.)
        imgui_pkg = self.dependencies["imgui"]
        path = imgui_pkg.package_folder.replace("\\", "/")
        cmake_file = os.path.join(self.build_folder, "imgui_package_path.cmake")
        with open(cmake_file, "w") as f:
            f.write('set(IMGUI_PACKAGE_FOLDER "{}")\n'.format(path))
