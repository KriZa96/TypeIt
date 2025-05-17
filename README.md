# **TypeIt**

**TypeIt** is a terminal-based touch typing game designed to test your typing skills. The game offers three base levels of difficulty. However, to keep things interesting, you can input custom text files for a personalized experience! While typing, you can see your accuracy and words per minute in real-time. The application is built with an interactive interface using the **FTXUI** library.

> **Note**: FTXUI does not support ćčšđž. When those letters are inputed
> nothing is shown but application calculates it as input, if you see
> that your input is shown as incorrect, just delete one character before
> you inputed ćčšđž -> [Online Test](https://arthursonzogni.github.io/FTXUI/examples/?file=component/input).

## **Technologies**

- **Language**: [C++20](https://isocpp.org/)
- **Testing Framework**: [Google Test](https://github.com/google/googletest)
- **Build System**: [CMake](https://cmake.org/)
- **Build Tool**: [Ninja](https://ninja-build.org/)
- **Package Management**: [vcpkg](https://github.com/microsoft/vcpkg)
- **UI Library**: [FTXUI](https://github.com/ArthurSonzogni/FTXUI)

## **Features**

- **Touch Typing Test**: Test your typing capabilities.
- **Levels**: Three difficulty levels (Simple, Medium, Hard) based on word frequency and length.
- **Custom Files**: Input your own textual files for personalized typing tests.
- **Predefined Timer**: Choose from 15s, 30s, or 60s test timers.
- **Custom Timer**: Define your own custom test session duration.
- **Statistics**: Track your typing accuracy and speed in real-time.
- **Error Check**: Incorrect characters are highlighted during typing.

## **Requirements**

Before building and running the project, ensure you have the following tools installed:

- **Git**: To clone the repository.
- **CMake >= 3.15**: To configure the project and manage dependencies.
- **Ninja**: The build tool used in this project. -> Linux
- **GCC/G++**: C and C++ compilers for Linux. -> Linux
- Visual Studio 2022 with C++ Development tools (Windows)

> **Note**: The C++ compiler must support the C++20 standard.

## **Installation**

> **Note**: Position yourself to root folder of the project.

****

### **Initialize Git Submodule**

Regardless of whether you're on Windows or Linux, you must initialize the Git submodule:

```bash
   git submodule update --init --recursive
```

****

### *LINUX*

Verify the installation of the required tools:

```bash
    git --version
    cmake --version
    ninja --version
    gcc --version
    g++ --version
```

*Setup CMake:*
```bash
  cmake -DCMAKE_MAKE_PROGRAM=ninja -G Ninja -S . -B cmake-build
```
*Build The Project:*
```bash
  cmake --build cmake-build 
```
*Add The Executable To Games Folder For Easier Access:*
```bash
  sudo ln -sfv "cmake-build/src/TypeIt" "/usr/games/TypeIt"
```
*Start The Game:*
```bash
  TypeIt
```
****

### *WINDOWS*

> **Note**: Ensure CMake is in your PATH

*Setup CMake:*
```shell
   cmake -G "Visual Studio 17 2022" -S . -B cmake-build 
```

*Build The Project:*
```shell
  cmake --build cmake-build 
```

*Move TypeIt.exe To Appropriate Place (Example copies to Desktop)<br>Or just start it from the folder noted in example*
```shell
  Copy-Item "cmake-build\src\Debug\TypeIt.exe" -Destination "$env:USERPROFILE\Desktop\"
```

# Speed Typing Application - Technical Guide

## Table of Contents

1. [Introduction](#introduction)
2. [Architecture Overview](#architecture-overview)
3. [Core Components](#core-components)
    - [Text Source Management](#text-source-management)
    - [Screen Management](#screen-management)
    - [Text Representation](#text-representation)
    - [Input Processing](#input-processing)
    - [Performance Measurement](#performance-measurement)
4. [User Interface Components](#user-interface-components)
    - [Main View](#main-view)
    - [Menu System](#menu-system)
    - [Speed Typing Session](#speed-typing-session)
    - [Text Input Area](#text-input-area)
    - [Performance Area](#performance-area)
5. [Engine Components](#engine-components)
    - [Input Accuracy Engine](#input-accuracy-engine)
    - [Input Line Engine](#input-line-engine)
    - [Input Word Count Engine](#input-word-count-engine)
    - [Word Calculator Engine](#word-calculator-engine)
6. [Data Structures](#data-structures)
    - [Game Options](#game-options)
    - [Game State](#game-state)
    - [Focus Position](#focus-position)
    - [Component Options](#component-options)
    - [Style](#style)
7. [Application Flow](#application-flow)
8. [Class Descriptions](#class-descriptions)
9. [Key Subsystems](#key-subsystems)
10. [UI Implementation](#ui-implementation)
11. [Input Processing Mechanics](#input-processing-mechanics)
12. [Performance Metrics](#performance-metrics)

## Introduction

This application is a terminal-based speed typing trainer built with the FTXUI (Functional Terminal User Interface) library. It allows users to practice and test their typing speed by providing different text samples, timing their typing session, and calculating metrics such as words per minute (WPM) and accuracy.

This guide provides an explanation of how the application works, its architecture, and some of the behaivour.

## Architecture Overview

### Design Patterns

1. **Composite Pattern**: Used extensively through FTXUI's component system, where UI elements are composed of smaller components.

2. **Strategy Pattern**: Different text sources can be swapped through the `ITextSource` interface.

3. **Decorator Pattern**: UI components are decorated with various styles and behaviors.

### Directory Structure

The application is organized into several directories: <br>Headers are in include while source files are in src folder

-   `/core`: Core functionality classes (`Text`, `Timer`, `Input`, etc.)
-   `/data`: Data structures and configuration (`GameOptions`, `GameState`, etc.)
-   `/engines`: Processing logic (`InputAccuracyEngine`, `WordCalculatorEngine`, etc.)
-   `/interface`: Abstract interfaces (`ITextSource`)
-   `/view`: UI components and screens (`Main`, `SpeedTypingSession`, etc.)

## Core Components

### Main Classes

1. **Main**: Entry point and controller for the application flow
2. **Screen**: Manages the terminal screen and refresh cycle
3. **Menu**: Handles the main menu UI and options
4. **SpeedTypingSession**: Coordinates a typing practice session
5. **Text**: Processes and displays the target text
6. **Input**: Manages user input and validation
7. **Timer**: Tracks session time
8. **WordCalculator**: Calculates typing speed metrics

### Key Interfaces

-   **ITextSource**: Abstract interface for text providers (allows extensibility)

## Application Flow

The application follows this high-level flow:

1. **Initialization**:

    - `Main` class instantiates and configures the application
    - Sets up the screen, main menu and handles game sessions

2. **Menu Navigation**:

    - User selects text difficulty (simple, medium, hard, or custom)
    - User selects time limit (15s, 30s, 60s, or custom)
    - User can view information or exit the application

3. **Game Session**:

    - When the user clicks "Start", a `SpeedTypingSession` is created
    - The application loads the selected text file using `FileTextSource`
    - The timer begins counting down from the selected time
    - The text is displayed for the user to type over

4. **Typing Process**:

    - As the user types, their input is processed character by character
    - Each character is compared to the target text character
    - Correct characters are displayed in grey/white, incorrect in salmon color
    - The application handles line breaks and word counting

5. **Performance Tracking**:

    - The application calculates and displays:
        - Time remaining
        - Words per minute (WPM)
        - Accuracy percentage

6. **Session End**:
    - Session ends when time expires or all text is typed
    - User can restart with Ctrl+R or return to menu with Ctrl+T

```
┌─────────────────────────────────────────┐
│              Application                │
│                  Start                  │
└───────────────────┬─────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────┐
│              Main Menu                  │
│   - Text selection                      │
│   - Time selection                      │
│   - Start/Exit options                  │
└───────────────────┬─────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────┐
│         SpeedTypingSession              │
│   - Timer starts                        │
│   - Text displayed                      │
│   - Input processing begins             │
└───────────────────┬─────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────┐
│         Input Processing                │
│   - Character validation                │
│   - Accuracy calculation                │
│   - Word counting                       │
└───────────────────┬─────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────┐
│         Performance Display             │
│   - WPM calculation                     │
│   - Accuracy percentage                 │
│   - Time remaining                      │
└───────────────────┬─────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────┐
│         Session End                     │
│   - Display results                     │
│   - Option to restart or exit           │
└─────────────────────────────────────────┘
```

## Core Components

### Text Source Management

#### ITextSource

-   **Purpose**: Interface defining the contract for text content providers.
-   **Key Methods**:
    -   `get_text()`: Retrieves the text content to be used in the typing session.

#### FileTextSource

-   **Purpose**: Implementation of ITextSource that reads text from files.
-   **Key Methods**:
    -   `get_text()`: Reads text from a file and returns it as a string.
    -   `is_file_valid()`: Validates if a file exists, is readable, and contains valid text.
-   **Functionality**:
    -   Handles file path validation
    -   Reads text content from the specified file

### Screen Management

#### Screen

-   **Purpose**: Manages the terminal screen interface and screen refresh cycles.
-   **Key Methods**:
    -   `start_refresh()`: Begins the screen refresh cycle.
    -   `stop_refresh()`: Stops the screen refresh cycle.
    -   `loop()`: Enters the main application loop with a given component.
    -   `get_screen_reference()`: Provides access to the underlying FTXUI screen object.
-   **Functionality**:
    -   Creates and manages the FTXUI ScreenInteractive instance
    -   Handles background thread for periodic UI refreshes (100ms interval)
    -   Provides a shutdown mechanism to stop the refresh thread

### Text Representation

#### Text

-   **Purpose**: Manages the text content display and formatting.
-   **Key Methods**:
    -   `get_text()`: Returns the raw text content.
    -   `get_text_lines()`: Returns the text content split into displayable lines.
    -   `get_text_component()`: Creates a renderable FTXUI component for the text.
    -   `get_char_at_line_and_position()`: Retrieves a character at a specific position in the text.
-   **Functionality**:
    -   Breaks text into visually manageable lines (first space after 55 characters or at newline character)
    -   Converts newline characters to spaces for continuous typing flow
    -   Stores both raw text and formatted text elements for rendering

### Input Processing

#### Input

-   **Purpose**: Manages user input and typing session interaction.
-   **Key Methods**:
    -   `get_input_component()`: Returns the renderable component for user input.
    -   `get_accuracy_component()`: Returns a component displaying typing accuracy.
    -   `get_word_count_reference()`: Provides access to the word count metric.
-   **Functionality**:
    -   Captures keyboard input from the user
    -   Processes input text and updates the input display
    -   Delegates to InputLineEngine for line management and InputWordCountEngine for metrics

### Performance Measurement

#### Timer

-   **Purpose**: Manages time tracking for typing sessions.
-   **Key Methods**:
    -   `start_timer()`: Begins the timer counting.
    -   `get_elapsed_time()`: Returns the elapsed time in seconds.
    -   `get_time_left_str()`: Returns a formatted string showing time remaining.
    -   `get_time_component()`: Returns a renderable component displaying time.
-   **Functionality**:
    -   Tracks session duration using chronos library
    -   Calculates remaining time based on the session time limit
    -   Signals finish of the game session if the time has elapsed

#### WordCalculator

-   **Purpose**: Calculates and displays words per minute metrics.
-   **Key Methods**:
    -   `get_word_calculator_component()`: Returns a renderable component displaying WPM.
    -   `get_word_per_minute_element()`: Creates the WPM display element.
-   **Functionality**:
    -   Uses WordCalculatorEngine to calculate WPM based on elapsed time and word count
    -   Formats the WPM metric for display

## User Interface Components

### Main View

#### Main

-   **Purpose**: Central component that coordinates the application's main views.
-   **Key Methods**:
    -   `Start()`: Static method to start the application.
    -   `start_main_session()`: Begins the main application loop.
    -   `refresh_game_session()`: Resets and refreshes the typing session.
    -   `get_main_component()`: Returns the main renderable component.
-   **Functionality**:
    -   Creates and manages the screen instance
    -   Coordinates between menu and speed typing session views
    -   Processes refresh and start session events

### Menu System

#### Menu

-   **Purpose**: Manages the application's main menu interface.
-   **Key Methods**:
    -   `get_menu_component()`: Returns the renderable menu component.
    -   `exit_application()`: Handles application exit.
    -   `get_text_radiobox_component_()`: Creates the text selection component.
    -   `get_time_radiobox_component_()`: Creates the time selection component.
-   **Functionality**:
    -   Provides text difficulty selection (simple, medium, hard, custom)
    -   Offers time limit options (15s, 30s, 60s, custom)
    -   Handles custom file path input for custom text selection
    -   Controls application flow (start session, exit, show info)

### Speed Typing Session

#### SpeedTypingSession

-   **Purpose**: Manages the active typing session and its components.
-   **Key Methods**:
    -   `get_speed_typing_session_component()`: Returns the renderable typing session component.
-   **Functionality**:
    -   Integrates all game session components (text display, input area, performance metrics)
    -   Coordinates between Text, Input, Timer, and WordCalculator components
    -   Handles session layout and styling

### Text Input Area

#### TextInputArea

-   **Purpose**: Manages the text display and input area.
-   **Key Methods**:
    -   `get_text_input_component()`: Returns the renderable text input component.
-   **Functionality**:
    -   Combines text display and input components
    -   Applies styling and layout to the input text area
    -   Displays "Session Finished!!!" when the game session ends

### Performance Area

#### PerformanceArea

-   **Purpose**: Manages the display of performance metrics.
-   **Key Methods**:
    -   `get_performance_component()`: Returns the renderable performance metrics component.
-   **Functionality**:
    -   Displays accuracy, timer, and WPM metrics in a styled box

## Engine Components

### Input Accuracy Engine

#### InputAccuracyEngine

-   **Purpose**: Calculates typing accuracy metrics.
-   **Key Methods**:
    -   `push_character_accuracy()`: Records whether a typed character was correct.
    -   `get_percentage_of_correct_input()`: Calculates and returns the typing accuracy percentage.
-   **Functionality**:
    -   Tracks correct and incorrect character inputs
    -   Calculates the percentage of correctly typed characters
    -   Provides accuracy metrics to the performance display

### Input Line Engine

#### InputLineEngine

-   **Purpose**: Manages the processing of input text across lines.
-   **Key Methods**:
    -   `render_input_text()`: Processes and renders the input text.
    -   `get_percentage_of_correct_input()`: Delegates to InputAccuracyEngine for accuracy metrics.
    -   `get_total_input_lines()`: Returns the rendered input lines.
-   **Functionality**:
    -   Tracks the current line and character position
    -   Compares input characters against the target text
    -   Manages line transitions when input exceeds line length
    -   Applies visual styling to indicate correct/incorrect input
    -   Coordinates with FocusPosition to maintain scroll through text

### Input Word Count Engine

#### InputWordCountEngine

-   **Purpose**: Tracks the number of words typed.
-   **Key Methods**:
    -   `set_word_count()`: Updates the word count based on input text.
    -   `get_word_count_reference()`: Provides access to the current word count.
-   **Functionality**:
    -   Parses input text to count words
    -   Updates the word count as new text is entered
    -   Provides word count metrics to the WordCalculator

### Word Calculator Engine

#### WordCalculatorEngine

-   **Purpose**: Calculates words per minute based on word count and elapsed time.
-   **Key Methods**:
    -   `get_words_per_minute_string()`: Returns a formatted WPM string.
    -   `calculate_words_per_minute()`: Calculates the WPM metric.
-   **Functionality**:
    -   Implements the WPM formula (60 \* words / seconds)
    -   Formats the WPM value for display
    -   Handles edge cases (such as elapsed time <= 1 second)

## Data Structures

### Game Options

#### GameOptions

-   **Purpose**: Stores application configuration options.
-   **Key Fields**:
    -   `time_radiobox_values_`: List of time options (15s, 30s, 60s, custom).
    -   `text_radiobox_values_`: List of text file path options.
    -   `selected_radiobox_time_`: Currently selected time option index.
    -   `selected_radiobox_text_`: Currently selected text option index.
    -   `custom_radiobox_input_`: Custom time input value.
-   **Functionality**:
    -   Provides centralized storage for application settings
    -   Defines default file paths for text samples
    -   Stores user selections for text and time options

### Game State

#### GameState

-   **Purpose**: Manages the state of the typing game.
-   **Key Fields**:
    -   `game_session_in_progress`: Whether a typing session is currently active.
    -   `start_session`: Flag to start a new session.
    -   `refresh_session`: Flag to refresh the current session.
    -   `game_finished`: Whether the current session has ended.
    -   `show_info`: Whether to display information panel.
-   **Key Methods**:
    -   `toggle_info_display()`: Toggles the information display visibility.
    -   `start_game_session()`: Initiates a new typing session.
-   **Functionality**:
    -   Controls application flow based on game state
    -   Validates requirements before starting a session
    -   Coordinates view transitions between menu and session

### Focus Position

#### FocusPosition

-   **Purpose**: Tracks cursor position for input focus. Current implementation behaivour only needs vertical position.
-   **Key Fields**:
    -   `x`: Horizontal position of the cursor.
    -   `y`: Vertical position of the cursor.
-   **Key Methods**:
    -   `reset()`: Resets cursor position to origin.
-   **Functionality**:
    -   Maintains cursor position for text input

### Component Options

#### ComponentOptions

-   **Purpose**: Defines UI component styling and behavior options.
-   **Key Fields**:
    -   `menu_radiobox_option`: Styling for radiobox elements.
    -   `menu_path_input_option`: Styling for file path input.
    -   `menu_time_input_option`: Styling for time input.
    -   `input_component_decorator`: Event handler for input components.
-   **Functionality**:
    -   Provides consistent styling for UI components
    -   Defines event handlers for keyboard input
    -   Implements validation feedback through color changes

### Style

#### Style

-   **Purpose**: Defines application-wide styling constants.
-   **Key Fields**:
    -   Color definitions for correct input, incorrect input, and default text
    -   Component size specifications
    -   Layout configurations (flexbox settings)
-   **Functionality**:
    -   Centralizes styling definitions
    -   Ensures consistent visual appearance across the application
    -   Defines layout strategies for component arrangement

## Key Subsystems

### Text Processing

The text processing system handles loading, displaying, and validating typed text:

1. **Loading**:

    - `FileTextSource` loads text from files
    - `Text` class processes it into lines and characters

2. **Display**:

    - Text is rendered with FTXUI components
    - Lines are wrapped at appropriate lengths

3. **Validation**:

    - `InputLineEngine` compares user input to target text
    - Characters are colored based on correctness

### User Input Handling

The input system processes keyboard input and manages the display of typed text:

1. **Capture**:

    - FTXUI's `Input` component captures keyboard input
    - Special key combinations (Ctrl+R, Ctrl+T) are handled if necessary
    - `Input::get_input_component()` triggers `InputLineEngine::render_input_text()`

2. **Processing**:

    - Characters are validated against the target text
    - `InputAccuracyEngine::push_character_accuracy()` records correctness.
    - The character is styled based on correctness.
    - Line breaks are handled automatically
    - `InputWordCountEngine::set_word_count()` updates the word count.
    - `WordCalculatorEngine::calculate_words_per_minute()` updates WPM.
    - `InputAccuracyEngine::get_percentage_of_correct_input()` updates accuracy.

3. **Display**:

    - Correct characters are shown in grey/white
    - Incorrect characters are shown in salmon color
    - Spaces are visually represented as underscores when incorrect

### Performance Metrics

The application tracks and displays several performance metrics:

1. **Timing**:

    - `Timer` class tracks elapsed and remaining time
    - Sessions end when time expires

2. **Speed**:

    - `WordCalculatorEngine` calculates words per minute
    - Calculation updates in real-time as user types

3. **Accuracy**:
    - `InputAccuracyEngine` calculates percentage of correct keystrokes
    - Updates in real-time as user types

## UI Implementation

The UI is built using the FTXUI library, which provides a component-based approach to terminal user interfaces:

### Key UI Components

1. **Menu Screen**:

    - Text difficulty selection (radiobox)
    - Time limit selection (radiobox)
    - Info, Start, and Exit buttons

2. **Typing Session Screen**:
    - Text display area (shows target text)
    - Input area (shows user typing with colored feedback)
    - Performance metrics bar (time, WPM, accuracy)
    - Session controls (Ctrl+R to restart, Ctrl+T for menu)

### Component Hierarchy

```
Main
├── Menu
│   ├── TextRadiobox
│   ├── PathInput (for custom text)
│   ├── TimeRadiobox
│   ├── TimeInput (for custom time)
│   ├── StartButton
│   ├── ExitButton
│   └── InfoButton
└── SpeedTypingSession
    ├── PerformanceArea
    │   ├── TimerComponent
    │   ├── AccuracyComponent
    │   └── WordCalculatorComponent
    ├── TextInputArea
    │   ├── TextComponent
    │   └── InputComponent
    └── SessionControls
```

### Rendering Process

1. The `Screen` class maintains a refresh thread that periodically triggers UI updates
2. Each component defines its rendering logic in its respective class
3. Components are composed hierarchically to form the complete UI
4. Conditional rendering (using `ftxui::Maybe`) is used to switch between menu and typing session

## Input Processing Mechanics

The application processes user input with sophisticated mechanics:

### Character-by-Character Processing

1. When a user types a character:
    - It's added to the `input_text_` string in the `Input` class
    - `InputLineEngine::render_input_text()` is called to process it
    - The character is compared to the corresponding character in the target text
    - A colored element is added to the display based on correctness

### Line Management

1. The application tracks the current line and position:

    - `FocusPosition` maintains the cursor position (line based)
    - `InputLineEngine` tracks the current line index

2. Line wrapping is handled in two ways:

    - Natural line breaks in the text
    - Automatic wrapping at 55 characters (first space after 55 characters)

3. When typing reaches the end of a line:
    - Current line is stored in `previous_input_lines_`
    - A new line is created for continued typing
    - Focus position is updated

### Backspace Handling

1. When the user presses backspace:
    - Character is removed from `input_text_`
    - `InputLineEngine::remove_element()` is called
    - If at the beginning of a line, it navigates to the previous line

## Performance Metrics

### Words Per Minute (WPM)

1. Calculation:

    - WPM = (Number of words typed × 60) ÷ Elapsed time in seconds
    - Implemented in `WordCalculatorEngine::calculate_words_per_minute()`

2. Word Counting:
    - Words are counted by splitting input text on whitespace
    - Implemented in `InputWordCountEngine::set_word_count()`

### Accuracy Percentage

1. Calculation:

    - Accuracy = (Number of correct characters ÷ Total characters typed) × 100%
    - Implemented in `InputAccuracyEngine::get_percentage_of_correct_input()`

2. Tracking:
    - Each character typed is recorded as correct/incorrect
    - Stored in `character_accuracy_` vector in `InputAccuracyEngine`

