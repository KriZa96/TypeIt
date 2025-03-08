
find_package(ftxui CONFIG REQUIRED)

add_executable(
        TypeIt
        main.cpp

        # Components
        components/Timer.h
        components/Timer.cpp
        components/Screen.h
        components/Screen.cpp
        components/WordCalculator.h
        components/WordCalculator.cpp

        # Text Sources
        text_sources/ITextSource.h
        text_sources/FileTextSource.h
        text_sources/FileTextSource.cpp
)

target_link_libraries(
        TypeIt PRIVATE
        ftxui::dom
        ftxui::screen
        ftxui::component
)
