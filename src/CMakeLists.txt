
find_package(ftxui CONFIG REQUIRED)

add_executable(
        TypeIt
        main.cpp
        components/Timer.h
        components/Timer.cpp
        components/Screen.h
        components/Screen.cpp
)

target_link_libraries(
        TypeIt PRIVATE
        ftxui::dom
        ftxui::screen
        ftxui::component
)
