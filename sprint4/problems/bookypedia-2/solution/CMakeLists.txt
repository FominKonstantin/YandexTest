cmake_minimum_required(VERSION 3.20)
project(bookypedia CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Это важно! CMakeDeps генерирует файлы, которые find_package использует
# Убедитесь, что у вас есть эти строки

# Поиск библиотек через Conan (CMakeDeps)
find_package(Boost REQUIRED CONFIG)  # CONFIG важен!
find_package(Catch2 REQUIRED CONFIG)
find_package(libpqxx REQUIRED CONFIG)
find_package(PostgreSQL REQUIRED CONFIG)

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