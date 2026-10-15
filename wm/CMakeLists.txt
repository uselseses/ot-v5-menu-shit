# wm — tiny "world model" — header-only.
# Houses ::wm::types::thread_safe_t<T,U> used by every bound control.
add_library(wm INTERFACE)
target_include_directories(wm INTERFACE include)
target_compile_features(wm INTERFACE cxx_std_20)
