cmake_minimum_required(VERSION 3.20)
project(PBL2_JPN LANGUAGES CXX)

add_library(pbl2_backend STATIC
    src/Application.cpp
    src/DataStore.cpp
    src/ApplicationService.cpp
    src/algorithms/KmpMatcher.cpp
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
    src/managers/UserManager.cpp
    src/managers/LevelManager.cpp
    src/managers/ResourceManager.cpp
    src/managers/PersonalManagers.cpp
    src/managers/StudyManagers.cpp
    src/storage/FileStorage.cpp
)
target_include_directories(pbl2_backend PUBLIC include)
target_compile_features(pbl2_backend PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(pbl2_backend PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(pbl2_backend PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(pbl2_app src/main.cpp)
target_link_libraries(pbl2_app PRIVATE pbl2_backend)

include(CTest)
if(BUILD_TESTING)
    add_executable(test_backend tests/test_backend.cpp)
    target_link_libraries(test_backend PRIVATE pbl2_backend)
    if(MSVC)
    target_compile_options(pbl2_app PRIVATE /utf-8)
endif()
    if(MSVC)
        target_compile_options(test_backend PRIVATE /utf-8 /W4)
    else()
        target_compile_options(test_backend PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME backend COMMAND test_backend)
endif()
