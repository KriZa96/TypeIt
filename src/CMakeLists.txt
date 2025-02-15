
find_package(ftxui CONFIG REQUIRED)

add_executable(TypeIt
        main.cpp
)

target_link_libraries(
        TypeIt PRIVATE
        ftxui::screen
        ftxui::dom
        ftxui::component
)
