# GUI — widget framework. Pure C++; pulls in Renderer for draw calls.
file(GLOB_RECURSE GUI_HEADERS CONFIGURE_DEPENDS include/GUI/*.hpp)
file(GLOB_RECURSE GUI_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(GUI STATIC ${GUI_HEADERS} ${GUI_SOURCES})
target_include_directories(GUI PUBLIC include ${CMAKE_SOURCE_DIR}/Cloud/include)
target_compile_features(GUI PUBLIC cxx_std_20)
target_link_libraries(GUI PUBLIC Renderer wm)
set_target_properties(GUI PROPERTIES FOLDER "libs")
