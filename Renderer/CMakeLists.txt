# Renderer — abstract base + DX11 backend (always) + optional D3D9 backend.
# Imports the original binary uses for the renderer subsystem:
#   d3d11, D3DCOMPILER_43, d3dx11_43
file(GLOB_RECURSE RENDERER_HEADERS CONFIGURE_DEPENDS include/Renderer/*.hpp)
file(GLOB_RECURSE RENDERER_SOURCES CONFIGURE_DEPENDS src/*.cpp)

# Opt-in D3D9 backend. Defaults ON because both backends are tiny and
# building them both keeps the library hostable in either kind of game.
# Disable with -DOTV5_WITH_D3D9=OFF if you don't have the d3d9 SDK.
option(OTV5_WITH_D3D9 "Build the D3D9 backend (c_d3d9_renderer + font_d3d9 + c_texture_d3d9)" ON)

if (NOT OTV5_WITH_D3D9)
    list(FILTER RENDERER_HEADERS EXCLUDE REGEX "_d3d9.hpp$")
    list(FILTER RENDERER_SOURCES EXCLUDE REGEX "_d3d9.cpp$")
endif ()

add_library(Renderer STATIC ${RENDERER_HEADERS} ${RENDERER_SOURCES})
target_include_directories(Renderer
    PUBLIC  include
    PRIVATE ${CMAKE_SOURCE_DIR}/third_party
)
target_compile_features(Renderer PUBLIC cxx_std_20)
target_link_libraries(Renderer
    PUBLIC  wm
    PRIVATE d3d11 d3dcompiler dxgi          # d3dcompiler resolves to D3DCOMPILER_47 in modern SDKs;
                                            # release will switch to D3DCOMPILER_43 to bit-match the binary
)
if (OTV5_WITH_D3D9)
    target_link_libraries(Renderer PRIVATE d3d9)
    target_compile_definitions(Renderer PUBLIC OTV5_WITH_D3D9=1)
endif ()
set_target_properties(Renderer PROPERTIES FOLDER "libs")
