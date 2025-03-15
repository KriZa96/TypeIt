
find_package(ftxui CONFIG REQUIRED)

file(
        GLOB
        SRC_FILES
        ./*/*.cpp
)

file(
        GLOB
        HEADER_FILES
        ../include/*/*.h
)

add_executable(
        TypeIt
        main.cpp
        ${HEADER_FILES}
        ${SRC_FILES}
)

target_link_libraries(
        TypeIt PRIVATE
        ftxui::dom
        ftxui::screen
        ftxui::component
)
