cmake_minimum_required(VERSION 3.16)
project(srms VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(SRMS_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_compile_definitions(_POSIX_C_SOURCE=200809L)

set(SRMS_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wshadow
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wwrite-strings
    -Wconversion
    -Wformat=2
)

set(SRMS_LIB_SOURCES
    src/status.c
    src/ascii.c
    src/buffer.c
    src/crc32.c
    src/student.c
    src/registry.c
    src/validate.c
    src/storage.c
    src/stats.c
    src/csv_io.c
    src/ui.c
    src/app.c
)

add_library(srms_lib STATIC ${SRMS_LIB_SOURCES})
target_include_directories(srms_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(srms_lib PRIVATE ${SRMS_WARNINGS})

add_executable(srms src/main.c)
target_link_libraries(srms PRIVATE srms_lib)
target_compile_options(srms PRIVATE ${SRMS_WARNINGS})

add_executable(srms_tests tests/test_main.c tests/harness.c)
target_include_directories(srms_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(srms_tests PRIVATE srms_lib)
target_compile_options(srms_tests PRIVATE ${SRMS_WARNINGS})

if(SRMS_SANITIZERS)
    foreach(tgt srms_lib srms srms_tests)
        target_compile_options(${tgt} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
            -fno-sanitize-recover=undefined)
        target_link_options(${tgt} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()

enable_testing()
add_test(NAME unit COMMAND srms_tests)
set_tests_properties(unit PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME cli COMMAND ${CMAKE_SOURCE_DIR}/tests/cli_smoke.sh $<TARGET_FILE:srms>)
set_tests_properties(cli PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
