
cmake_minimum_required(VERSION 3.20)

project(PBL2_JPN VERSION 0.1.0 LANGUAGES CXX)

# Common compiler warnings
function(enable_project_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /permissive- /utf-8
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()
endfunction()

# Domain library
add_library(pbl2_domain STATIC
    src/domain/vocabulary.cpp
)

target_include_directories(pbl2_domain
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(pbl2_domain
    PUBLIC cxx_std_17
)

set_target_properties(pbl2_domain PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

enable_project_warnings(pbl2_domain)

# CLI application
add_executable(pbl2_app
    src/main.cpp
    src/storage/FileStorage.cpp
)

target_link_libraries(pbl2_app
    PRIVATE pbl2_domain
)

enable_project_warnings(pbl2_app)

# Automated testing
include(CTest)

if(BUILD_TESTING)
    add_executable(test_vocabulary
        tests/test_vocabulary.cpp
    )

    target_link_libraries(test_vocabulary
        PRIVATE pbl2_domain
    )

    enable_project_warnings(test_vocabulary)

    add_test(
        NAME domain.vocabulary
        COMMAND test_vocabulary
    )
        # Kiem thu LinkedList
    add_executable(test_linked_list
        tests/test_linked_list.cpp
    )

    target_link_libraries(test_linked_list
        PRIVATE pbl2_domain
    )

    enable_project_warnings(test_linked_list)

    add_test(
        NAME dsa.linked_list
        COMMAND test_linked_list
    )

    # Kiem thu B2: doc Vocabulary tu TXT
    add_executable(test_B2
        tests/test_B2.cpp
        src/storage/FileStorage.cpp
    )

    target_link_libraries(test_B2
        PRIVATE pbl2_domain
    )

    enable_project_warnings(test_B2)

    add_test(
        NAME storage.B2
        COMMAND test_B2
    )
endif()
