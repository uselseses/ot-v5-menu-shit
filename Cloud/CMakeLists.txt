# Cloud — config sync client over raw WS2_32 + tiny-AES.
# Defines GUI::config_system (create/delete/save/load/etc.) and
# the cloud-to-GUI bridge introduced in the Jul 12-14 changelog.
file(GLOB_RECURSE CLOUD_HEADERS CONFIGURE_DEPENDS include/Cloud/*.hpp)
file(GLOB_RECURSE CLOUD_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(Cloud STATIC ${CLOUD_HEADERS} ${CLOUD_SOURCES})
target_include_directories(Cloud PUBLIC include)
target_compile_features(Cloud PUBLIC cxx_std_20)
target_link_libraries(Cloud
    PUBLIC  GUI
    PRIVATE ws2_32
)
set_target_properties(Cloud PROPERTIES FOLDER "libs")
