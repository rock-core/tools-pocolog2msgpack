Autobuild::Package.each do |name, pkg|
    if pkg.kind_of?(Autobuild::CMake)
        pkg.define "CMAKE_BUILD_TYPE", "Release"
        pkg.define "CMAKE_CXX_FLAGS", "-std=c++11"
    end
end

