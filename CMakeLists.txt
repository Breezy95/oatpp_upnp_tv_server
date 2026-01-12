cmake_minimum_required(VERSION 3.11)
set(project_name upnptvserver) 
set(server_address http://192.168.0.212:8000)
set(CMAKE_BUILD_TYPE Debug)
project(${project_name})

set(CMAKE_CXX_STANDARD 17)
include(FetchContent)

file(GLOB CONTROLLER_SRC "src/controller/*.cpp" "src/controller/*.hpp")
add_library(${project_name}-lib
        src/AppComponent.hpp
        src/dto/DTOs.hpp
        src/ixmlfuncs.cpp
        src/ixmlfuncs.hpp
        ${CONTROLLER_SRC}
)

#FetchContent_Declare(pugixml GIT_REPOSITORY https://github.com/zeux/pugixml.git GIT_TAG latest)
#FetchContent_MakeAvailable(pugixml)
# Find Oat++ and pupnp


find_package(oatpp 1.3.0 REQUIRED)
find_package(oatpp-ssdp 1.3.0 REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(LibXml2 REQUIRED)
pkg_check_modules(PUPNP REQUIRED IMPORTED_TARGET libupnp)
pkg_check_modules(GLIB IMPORTED_TARGET glib-2.0)
pkg_check_modules(LIBXML2 REQUIRED IMPORTED_TARGET libxml-2.0)
pkg_check_modules(GUPNP_AV IMPORTED_TARGET gupnp-av-1.0)
pkg_check_modules(GUPNP_DLNA IMPORTED_TARGET gupnp-dlna-2.0)
pkg_check_modules(FFM_LIBAVFORMAT IMPORTED_TARGET libavformat)
pkg_check_modules(FFM_LIBAVCODEC IMPORTED_TARGET libavcodec)
pkg_check_modules(FFM_LIBAVUTIL IMPORTED_TARGET libavutil)


target_link_libraries(${project_name}-lib
                #PUBLIC pugixml::pugi
                PUBLIC oatpp::oatpp
                PUBLIC oatpp::oatpp-test
                PUBLIC oatpp::oatpp-ssdp
                PkgConfig::PUPNP
                
)

# GLib is required for gupnp; fail early if missing
if(TARGET PkgConfig::GLIB)
        target_link_libraries(${project_name}-lib PUBLIC PkgConfig::GLIB)
else()
        message(FATAL_ERROR "glib-2.0 pkg-config package not found; please install libglib2.0-dev or equivalent")
endif()

if(TARGET PkgConfig::GUPNP_DLNA)
    target_link_libraries(${project_name}-lib PUBLIC PkgConfig::GUPNP_DLNA)
else()
    message(FATAL_ERROR "gupnp-dlna-2.0 pkg-config package not found; please install libgupnp-dlna-2.0-dev")
endif()

# libxml2 is required by gupnp DIDL headers
if(TARGET PkgConfig::LIBXML2)
        target_link_libraries(${project_name}-lib PUBLIC PkgConfig::LIBXML2)
else()
        message(FATAL_ERROR "libxml-2.0 pkg-config package not found; please install libxml2-dev or equivalent")
endif()

# FFmpeg libs: link if present
if(TARGET PkgConfig::FFM_LIBAVFORMAT)
        target_link_libraries(${project_name}-lib PUBLIC PkgConfig::FFM_LIBAVFORMAT)
else()
        message(WARNING "libavformat not found; ffmpeg format support disabled")
endif()

if(TARGET PkgConfig::FFM_LIBAVCODEC)
        target_link_libraries(${project_name}-lib PUBLIC PkgConfig::FFM_LIBAVCODEC)
else()
        message(WARNING "libavcodec not found; ffmpeg codec support disabled")
endif()

if(TARGET PkgConfig::FFM_LIBAVUTIL)
        target_link_libraries(${project_name}-lib PUBLIC PkgConfig::FFM_LIBAVUTIL)
else()
        message(WARNING "libavutil not found; ffmpeg util support disabled")
endif()

add_definitions(
        -DEXAMPLE_MEDIA_FOLDER="${CMAKE_CURRENT_LIST_DIR}/video"
        -DSERVER_ADDRESS="${server_address}"
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


