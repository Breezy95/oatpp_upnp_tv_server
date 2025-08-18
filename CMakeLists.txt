cmake_minimum_required(VERSION 3.11)
set(project_name upnptvserver) 

project(${project_name})

set(CMAKE_CXX_STANDARD 17)
include(FetchContent)

file(GLOB CONTROLLER_SRC "src/controller/*.cpp" "src/controller/*.hpp")
add_library(${project_name}-lib
        src/AppComponent.hpp
        src/dto/DTOs.hpp
        src/ixmlfuncs.cpp
        ${CONTROLLER_SRC}
)

#FetchContent_Declare(pugixml GIT_REPOSITORY https://github.com/zeux/pugixml.git GIT_TAG latest)
#FetchContent_MakeAvailable(pugixml)
# Find Oat++ and pupnp
find_package(oatpp 1.3.0 REQUIRED)
find_package(oatpp-ssdp 1.3.0 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PUPNP REQUIRED IMPORTED_TARGET libupnp)

target_link_libraries(${project_name}-lib
        #PUBLIC pugixml::pugi
        PUBLIC oatpp::oatpp
        PUBLIC oatpp::oatpp-test
        PUBLIC oatpp::oatpp-ssdp
        PkgConfig::PUPNP
)

add_definitions(
        -DEXAMPLE_MEDIA_FOLDER="${CMAKE_CURRENT_LIST_DIR}/video"
)

target_include_directories(${project_name}-lib PUBLIC src)

add_executable(${project_name}-exe
        src/App.cpp
        test/app/MyApiTestClient.hpp)

target_link_libraries(${project_name}-exe ${project_name}-lib)
add_dependencies(${project_name}-exe ${project_name}-lib)

#testing
add_executable(${project_name}-test
        test/tests.cpp
        test/app/TestComponent.hpp
        test/app/MyApiTestClient.hpp
        test/TestController.cpp
        test/TestController.hpp
)

target_link_libraries(${project_name}-test ${project_name}-lib)
add_dependencies(${project_name}-test ${project_name}-lib)

set_target_properties(${project_name}-lib ${project_name}-exe ${project_name}-test PROPERTIES
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
)

enable_testing()
add_test(project-tests ${project_name}-test)


