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
    src/domain/QuizSession.cpp
    src/domain/Time.cpp
    src/managers/UserManager.cpp
    src/managers/LevelManager.cpp
    src/managers/ResourceManager.cpp
    src/managers/PersonalManagers.cpp
    src/managers/AlphabetManager.cpp
    src/managers/QuestionManager.cpp
    src/managers/QuizManager.cpp
    src/storage/FileStorage.cpp
    src/storage/Paths.cpp
)
target_include_directories(pbl2_backend PUBLIC include)
target_compile_features(pbl2_backend PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(pbl2_backend PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(pbl2_backend PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(test_backend tests/test_backend.cpp tests/InternalApplication.cpp)
    target_link_libraries(test_backend PRIVATE pbl2_backend)
    if(MSVC)
        target_compile_options(test_backend PRIVATE /utf-8 /W4)
    else()
        target_compile_options(test_backend PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME backend COMMAND test_backend)
    foreach(group algorithms storage api search)
        add_executable(test_${group} tests/test_${group}.cpp)
        target_link_libraries(test_${group} PRIVATE pbl2_backend)
        if(MSVC)
            target_compile_options(test_${group} PRIVATE /utf-8 /W4)
        else()
            target_compile_options(test_${group} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME ${group} COMMAND test_${group})
    endforeach()
endif()

option(PBL2_BUILD_EXAMPLE "Build the isolated frontend API consumer" OFF)
if(PBL2_BUILD_EXAMPLE)
    add_executable(backend_example UI/backend_example.cpp)
    target_link_libraries(backend_example PRIVATE pbl2_backend)
    if(MSVC)
        target_compile_options(backend_example PRIVATE /utf-8)
    endif()
endif()
