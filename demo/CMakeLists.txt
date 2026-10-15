# demo — sandbox harness. Reproduces GUI_64_DX11.exe.
# WIN32 subsystem (matches PE subsystem 2 of the original).
file(GLOB DEMO_SOURCES CONFIGURE_DEPENDS *.cpp *.hpp)

add_executable(demo WIN32 ${DEMO_SOURCES})
target_link_libraries(demo
    PRIVATE GUI Renderer Cloud wm
            kernel32 user32        # explicit, to mirror imports.json exactly
)
target_compile_definitions(demo PRIVATE
    OTV5_ASSET_DIR="${CMAKE_SOURCE_DIR}/exported_textures"
)
set_target_properties(demo PROPERTIES
    OUTPUT_NAME "GUI_64_DX11"      # match the original binary name
    FOLDER      "exe"
)
