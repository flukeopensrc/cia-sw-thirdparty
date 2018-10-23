
project(usbtmc_test)

# Header files
set(USBTMC_TEST_HEADERS
)

# Source files
set(USBTMC_TEST_SOURCES
    usbtmc488_test.c
)

source_group("Source Files" FILES ${USBTMC_TEST_HEADERS} ${USBTMC_TEST_SOURCES})

include_directories(
  ../src
)

add_definitions(-DLINUX -D_POSIX_TIMERS -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE)

add_executable(usbtmc_test
    ${USBTMC_TEST_HEADERS} ${USBTMC_TEST_SOURCES}
)

target_link_libraries(usbtmc_test
    usbtmc
    pthread
)





