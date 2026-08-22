add_library(game_model STATIC
    src/model.cpp
    src/game_state.cpp
    src/loot_generator.cpp
    src/collision_detector.cpp
)

target_include_directories(game_model
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(game_model
    PUBLIC
        CONAN_PKG::boost
        CONAN_PKG::libpqxx          # <-- ДОБАВИТЬ
)

set_property(TARGET game_model PROPERTY CXX_STANDARD 20)