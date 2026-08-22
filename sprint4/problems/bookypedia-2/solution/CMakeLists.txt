cmake_minimum_required(VERSION 3.11)
project(bookypedia CXX)
set(CMAKE_CXX_STANDARD 20)

# Проверяем, существует ли conanbuildinfo_multi.cmake
if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    conan_basic_setup(TARGETS)
else()
    # Используем find_package напрямую
    find_package(Boost REQUIRED)
    find_package(Catch2 REQUIRED)
    find_package(libpqxx REQUIRED)
    find_package(PostgreSQL REQUIRED)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(libbookypedia STATIC
    src/menu/menu.cpp
    src/menu/menu.h
    src/ui/view.cpp
    src/ui/view.h
    src/app/use_cases.h
    src/app/use_cases_impl.cpp
    src/app/use_cases_impl.h
    src/domain/author.cpp
    src/domain/author.h
    src/domain/author_fwd.h
    src/domain/book.h
    src/domain/book_tag.h
    src/domain/command_repository.h
    src/util/tagged.h
    src/util/tagged_uuid.cpp
    src/util/tagged_uuid.h
    src/postgres/postgres.cpp
    src/postgres/postgres.h
)

target_link_libraries(libbookypedia PUBLIC 
    Boost::boost 
    Threads::Threads 
    PostgreSQL::PostgreSQL
    libpqxx::libpqxx
)

add_executable(bookypedia
    src/bookypedia.cpp
    src/bookypedia.h
    src/main.cpp
)

target_link_libraries(bookypedia PRIVATE 
    Boost::boost 
    libbookypedia
)

add_executable(tests
    tests/use_case_tests.cpp
    tests/tagged_uuid_tests.cpp
)

target_link_libraries(tests PRIVATE 
    Catch2::Catch2 
    libbookypedia
)