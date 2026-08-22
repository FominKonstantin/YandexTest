cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

if(UNIX AND NOT WIN32)
    add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(lib)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/json_loader.cpp
    src/boost_json.cpp
    src/state_manager.cpp          
    src/state_serialization.cpp   
    src/record_manager.cpp         # <-- ДОБАВИТЬ
    src/ticker.h
    src/api_handlers.h   
    src/response_helpers.h 
    src/serializers.h       
)

target_include_directories(game_server
    PRIVATE
        src
        lib/include
)

target_link_libraries(game_server
    PRIVATE
        game_model
        CONAN_PKG::boost
        CONAN_PKG::libpqxx          # <-- ДОБАВИТЬ
)

if(UNIX AND NOT WIN32)
    target_link_libraries(game_server
        PRIVATE
            pthread
            dl
            rt
    )
endif()

# Тесты
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/loot_generator_tests.cpp AND 
   EXISTS ${CMAKE_SOURCE_DIR}/tests/state-serialization-tests.cpp)
   
    add_executable(game_server_tests
        src/json_loader.cpp
        src/boost_json.cpp
        src/state_serialization.cpp  
        tests/loot_generator_tests.cpp
        tests/state-serialization-tests.cpp  
    )

    target_include_directories(game_server_tests
        PRIVATE
            src
            lib/include
            ${CONAN_INCLUDE_DIRS_BOOST}
    )

    target_link_libraries(game_server_tests
        PRIVATE
            game_model
            CONAN_PKG::catch2
            CONAN_PKG::boost
            Threads::Threads
    )

    if(UNIX AND NOT WIN32)
        target_link_libraries(game_server_tests
            PRIVATE
                pthread
                dl
                rt
        )
    endif()

    enable_testing()
    add_test(NAME game_server_tests COMMAND game_server_tests)
endif()