cmake_minimum_required(VERSION 3.16)

option(SWITCH_WIFI_BUILD_APP "Build the Nintendo Switch Borealis application" ON)
option(SWITCH_WIFI_BUILD_TESTS "Build host-side unit tests" OFF)
option(SWITCH_WIFI_ENABLE_SANITIZERS "Enable ASan/UBSan for host tests" OFF)
option(SWITCH_WIFI_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)

# Borealis selects the devkitPro cross-toolchain before project(). Host tests are
# deliberately independent so contributors can run them without devkitPro.
if (SWITCH_WIFI_BUILD_APP)
    if (NOT PLATFORM_SWITCH)
        message(FATAL_ERROR "switch-wifi app targets Nintendo Switch. Pass -DPLATFORM_SWITCH=ON, or set SWITCH_WIFI_BUILD_APP=OFF for host tests.")
    endif()
    set(BOREALIS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/library)
    if (NOT EXISTS ${BOREALIS_LIBRARY}/CMakeLists.txt)
        message(FATAL_ERROR "Borealis submodule missing. Run: git submodule update --init --recursive")
    endif()
    include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)
    include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)
endif()

project(switch-wifi VERSION 0.3.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(switch_wifi_core STATIC src/core/metrics.cpp)
target_include_directories(switch_wifi_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(switch_wifi_core PUBLIC cxx_std_17)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(switch_wifi_core PRIVATE -Wall -Wextra -Wpedantic)
    if (SWITCH_WIFI_WARNINGS_AS_ERRORS)
        target_compile_options(switch_wifi_core PRIVATE -Werror)
    endif()
endif()

if (SWITCH_WIFI_BUILD_TESTS)
    enable_testing()

    function(add_switch_wifi_test test_name source_file)
        add_executable(${test_name} ${source_file})
        target_link_libraries(${test_name} PRIVATE switch_wifi_core)
        if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            # Unit tests intentionally use assert(); Release builds define
            # NDEBUG globally, so explicitly keep assertions active in tests.
            target_compile_options(${test_name} PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
            if (SWITCH_WIFI_WARNINGS_AS_ERRORS)
                target_compile_options(${test_name} PRIVATE -Werror)
            endif()
            if (SWITCH_WIFI_ENABLE_SANITIZERS)
                target_compile_options(${test_name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
                target_link_options(${test_name} PRIVATE -fsanitize=address,undefined)
            endif()
        endif()
        add_test(NAME ${test_name} COMMAND ${test_name})
    endfunction()

    add_switch_wifi_test(switch_wifi_metrics_tests tests/test_metrics.cpp)
    add_switch_wifi_test(switch_wifi_history_tests tests/test_history.cpp)
    add_switch_wifi_test(switch_wifi_benchmark_tests tests/test_benchmark.cpp)
endif()

if (SWITCH_WIFI_BUILD_APP)
    # libnx does not expose the POSIX file APIs used by fmt's optional os.cc.
    # Borealis only needs fmt's core formatting support on Nintendo Switch.
    set(FMT_OS OFF CACHE BOOL "Disable unsupported fmt OS APIs for libnx" FORCE)

    set(PACKAGE_NAME "ca.antoine.switchwifi")
    set(PROJECT_AUTHOR "Antoine Boucher")
    set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.jpg)
    set(BOREALIS_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/resources)
    set(PROJECT_RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/romfs)
    file(GLOB_RECURSE PROJECT_RESOURCE_FILES CONFIGURE_DEPENDS "${BOREALIS_RESOURCES}/*")
    file(COPY "${BOREALIS_RESOURCES}/" DESTINATION "${PROJECT_RESOURCES}")
    file(MAKE_DIRECTORY "${PROJECT_RESOURCES}/img")
    configure_file("${PROJECT_ICON}" "${PROJECT_RESOURCES}/img/switch_wifi.jpg" COPYONLY)

    add_subdirectory(library)

    find_package(Threads REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBCURL REQUIRED libcurl)

    file(GLOB_RECURSE APP_SRC CONFIGURE_DEPENDS src/*.cpp)
    list(REMOVE_ITEM APP_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/core/metrics.cpp)
    list(APPEND APP_SRC ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)

    set(APP_PLATFORM_LIB glfw3 EGL glapi drm_nouveau nx m ${CMAKE_THREAD_LIBS_INIT})

    program_target(${PROJECT_NAME} "${APP_SRC}")
    set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS OFF)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LIBCURL_INCLUDE_DIRS})
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -ffunction-sections -fdata-sections ${LIBCURL_CFLAGS_OTHER})
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        SWITCH_WIFI_VERSION=\"${PROJECT_VERSION}\"
        SWITCH_WIFI_ATMOSPHERE_TARGET=\"1.11.2\"
        SWITCH_WIFI_HOS_TARGET=\"22.5.0\"
        SWITCH_WIFI_LIBNX_BASELINE=\"4.10.0+\")
    target_link_directories(${PROJECT_NAME} PRIVATE ${LIBCURL_LIBRARY_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE
        switch_wifi_core borealis ${APP_PLATFORM_LIB} ${LIBCURL_LIBRARIES})

    set(NACP_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.nacp)
    set(NRO_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.nro)
    add_custom_command(
        OUTPUT ${NRO_OUTPUT}
        COMMAND ${NX_NACPTOOL_EXE} --create "switch-wifi" "${PROJECT_AUTHOR}" "${PROJECT_VERSION}" ${NACP_OUTPUT}
        COMMAND ${NX_ELF2NRO_EXE} ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.elf ${NRO_OUTPUT}
                --icon=${PROJECT_ICON} --nacp=${NACP_OUTPUT} --romfsdir=${PROJECT_RESOURCES}
        DEPENDS ${PROJECT_NAME} ${PROJECT_ICON} ${PROJECT_RESOURCE_FILES}
        COMMENT "Packaging ${PROJECT_NAME}.nro"
        VERBATIM)
    add_custom_target(${PROJECT_NAME}.nro DEPENDS ${NRO_OUTPUT})
endif()
