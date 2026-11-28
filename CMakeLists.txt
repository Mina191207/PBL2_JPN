cmake_minimum_required(VERSION 3.20)
project(PBL2_JPN LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    find_package(unofficial-sodium CONFIG QUIET)
    set(sodium_target unofficial-sodium::sodium)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SODIUM QUIET IMPORTED_TARGET libsodium>=1.0.18)
    endif()
    set(sodium_target PkgConfig::SODIUM)
endif()
if(NOT TARGET ${sodium_target})
    message(FATAL_ERROR "libsodium >= 1.0.18 required. Debian/Ubuntu: apt install libsodium-dev pkg-config. MSYS2 UCRT64: pacman -S mingw-w64-ucrt-x86_64-libsodium mingw-w64-ucrt-x86_64-pkgconf. MSVC: vcpkg install libsodium:x64-windows and configure with its CMake toolchain.")
endif()

add_library(pbl2_backend STATIC
    src/application/Application.cpp
    src/storage/DataStore.cpp
    src/application/ApplicationService.cpp
    src/domain/resource.cpp
    src/domain/level.cpp
    src/domain/vocabulary.cpp
    src/domain/grammar.cpp
    src/domain/kanji.cpp
    src/domain/user.cpp
    src/domain/favorite.cpp
    src/domain/search_his.cpp
    src/domain/AlphabetEntry.cpp
    src/domain/Question.cpp
    src/domain/QuizAttempt.cpp
    src/domain/Validation.cpp
    src/domain/EntityId.cpp
    src/domain/ReviewSession.cpp
    src/domain/QuizSession.cpp
    src/domain/Time.cpp
    src/application/managers/UserManager.cpp
    src/application/managers/LevelManager.cpp
    src/application/managers/ResourceManager.cpp
    src/application/managers/PersonalManagers.cpp
    src/application/managers/AlphabetManager.cpp
    src/application/managers/QuestionManager.cpp
    src/application/managers/QuizManager.cpp
    src/storage/FileStorage.cpp
    src/storage/Paths.cpp
    src/search/ResourceSearch.cpp
)
target_include_directories(pbl2_backend PUBLIC include PRIVATE src)
target_compile_features(pbl2_backend PUBLIC cxx_std_17)
target_link_libraries(pbl2_backend PRIVATE ${sodium_target})
if(WIN32)
    target_link_libraries(pbl2_backend PRIVATE advapi32)
endif()
if(MSVC)
    target_compile_options(pbl2_backend PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(pbl2_backend PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(pbl2_migrate_users src/storage/migrate_users.cpp)
target_include_directories(pbl2_migrate_users PRIVATE src)
target_link_libraries(pbl2_migrate_users PRIVATE pbl2_backend)
if(WIN32 AND MINGW)
    target_link_options(pbl2_migrate_users PRIVATE -municode)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(test_backend tests/test_backend.cpp tests/support/InternalApplication.cpp)
    target_include_directories(test_backend PRIVATE src tests/support)
    target_link_libraries(test_backend PRIVATE pbl2_backend)
    if(MSVC)
        target_compile_options(test_backend PRIVATE /utf-8 /W4)
    else()
        target_compile_options(test_backend PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME backend COMMAND test_backend)
    foreach(group algorithms storage api search credentials)
        add_executable(test_${group} tests/test_${group}.cpp)
        target_include_directories(test_${group} PRIVATE src tests/support)
        target_link_libraries(test_${group} PRIVATE pbl2_backend)
        if(MSVC)
            target_compile_options(test_${group} PRIVATE /utf-8 /W4)
        else()
            target_compile_options(test_${group} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME ${group} COMMAND test_${group})
    endforeach()
    target_link_libraries(test_credentials PRIVATE ${sodium_target})
    target_include_directories(test_credentials PRIVATE apps/raylib)
    target_include_directories(test_search PRIVATE apps/raylib)
endif()

option(PBL2_BUILD_EXAMPLE "Build the isolated frontend API consumer" OFF)
option(PBL2_BUILD_SEARCH_BENCHMARK "Build the synthetic search benchmark" OFF)
if(PBL2_BUILD_SEARCH_BENCHMARK)
    add_executable(search_benchmark tests/search_benchmark.cpp)
    target_include_directories(search_benchmark PRIVATE src tests/support)
    target_link_libraries(search_benchmark PRIVATE pbl2_backend)
    if(MSVC)
        target_compile_options(search_benchmark PRIVATE /utf-8)
    endif()
endif()

if(PBL2_BUILD_EXAMPLE)
    add_executable(backend_example examples/backend_example.cpp)
    target_link_libraries(backend_example PRIVATE pbl2_backend)
    if(MSVC)
        target_compile_options(backend_example PRIVATE /utf-8)
    endif()
    if(WIN32 AND MINGW)
        target_link_options(backend_example PRIVATE -municode)
    endif()
    if(BUILD_TESTING)
        add_test(NAME workflow COMMAND backend_example)
    endif()
endif()

# ==========================================
# Raylib Frontend
# ==========================================

option(PBL2_BUILD_RAYLIB "Build Raylib frontend" OFF)

if(PBL2_BUILD_RAYLIB)
    find_package(raylib CONFIG REQUIRED)

    add_executable(pbl2_app
        apps/raylib/main.cpp
        apps/raylib/UiText.cpp
        apps/raylib/UITheme.cpp
        apps/raylib/app/AppController.cpp
        apps/raylib/app/AdminActions.cpp
        apps/raylib/app/ResourceActions.cpp
        apps/raylib/app/StudyActions.cpp
        apps/raylib/screens/StartupScreen.cpp
        apps/raylib/screens/LoginScreen.cpp
        apps/raylib/screens/MainScreen.cpp
        apps/raylib/screens/ResourcesScreen.cpp
        apps/raylib/screens/BackendScreen.cpp
        apps/raylib/screens/StudyScreen.cpp
        apps/raylib/screens/AdminForm.cpp
    )

    target_include_directories(pbl2_app PRIVATE apps/raylib)
    if(WIN32 AND MINGW)
        target_link_options(pbl2_app PRIVATE -municode)
    endif()
    target_link_libraries(pbl2_app PRIVATE
        pbl2_backend
        raylib
    )

    add_custom_command(TARGET pbl2_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:pbl2_app>/assets/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/NotoSansJP-Medium.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/OFL.txt"
            "$<TARGET_FILE_DIR:pbl2_app>/assets/fonts"
        COMMENT "Copying UI font assets beside pbl2_app"
    )

    if(MSVC)
        target_compile_options(pbl2_app PRIVATE /W4 /utf-8)
    else()
        target_compile_options(pbl2_app PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(BUILD_TESTING)
        add_executable(test_raylib_state tests/test_raylib_state.cpp
            apps/raylib/screens/BackendScreen.cpp
            apps/raylib/screens/StudyScreen.cpp
            apps/raylib/screens/AdminForm.cpp
            apps/raylib/screens/ResourcesScreen.cpp
            apps/raylib/screens/StartupScreen.cpp
            apps/raylib/UITheme.cpp
            apps/raylib/UiText.cpp
        )
        target_include_directories(test_raylib_state PRIVATE apps/raylib tests/support)
        target_link_libraries(test_raylib_state PRIVATE pbl2_backend raylib)
        foreach(input GetScreenWidth GetScreenHeight GetMousePosition IsMouseButtonPressed IsKeyPressed
                IsKeyPressedRepeat IsKeyDown GetMouseWheelMove GetClipboardText GetCharPressed)
            target_compile_definitions(test_raylib_state PRIVATE ${input}=Test${input})
        endforeach()
        if(MSVC)
            target_compile_options(test_raylib_state PRIVATE /utf-8)
        endif()
        add_test(NAME raylib_state COMMAND test_raylib_state)
    endif()
endif()
