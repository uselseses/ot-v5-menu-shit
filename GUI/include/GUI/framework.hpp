#pragma once
// Umbrella header --- pulls in the public surface of the GUI library.
// Demo code does `#include <GUI/framework.hpp>` and gets everything.

#include <GUI/build_options.hpp>

#include <GUI/core/c_control_interface.hpp>
#include <GUI/core/element_factory.hpp>
#include <GUI/core/focus.hpp>
#include <GUI/core/globals.hpp>
#include <GUI/core/gui_engine_interface.hpp>
#include <GUI/core/hash.hpp>

#include <GUI/elements/c_base_control.hpp>
#include <GUI/elements/c_base_element.hpp>
#include <GUI/elements/c_body.hpp>
#include <GUI/elements/c_button.hpp>
#include <GUI/elements/c_button_grid.hpp>
#include <GUI/elements/c_checkbox.hpp>
#include <GUI/elements/c_colorpicker.hpp>
#include <GUI/elements/c_column.hpp>
#include <GUI/elements/c_dropdown.hpp>
#include <GUI/elements/c_groupbox.hpp>
#include <GUI/elements/c_keybinder.hpp>
#include <GUI/elements/c_label.hpp>
#include <GUI/elements/c_listbox.hpp>
#include <GUI/elements/c_listbox_item.hpp>
#include <GUI/elements/c_scrollable_element.hpp>
#include <GUI/elements/c_separator.hpp>
#include <GUI/elements/c_slider.hpp>
#include <GUI/elements/c_text_container.hpp>
#include <GUI/elements/c_textbox_new.hpp>

#include <GUI/containers/c_base_window.hpp>
#include <GUI/containers/c_context_menu.hpp>
#include <GUI/containers/c_context_menu_owner.hpp>
#include <GUI/containers/c_dialog_window.hpp>
#include <GUI/containers/c_menu_window.hpp>
#include <GUI/containers/c_page.hpp>
#include <GUI/containers/c_page_manager.hpp>
#include <GUI/containers/c_subtab.hpp>
#include <GUI/containers/c_tab.hpp>
#include <GUI/containers/c_tab_manager.hpp>

#include <GUI/enums/antiaim_condition_e.hpp>
#include <GUI/enums/chams_style_e.hpp>
#include <GUI/enums/keybinder_mode_e.hpp>
#include <GUI/enums/watermark_flag_e.hpp>
#include <GUI/enums/all_other_enums.hpp>

#include <GUI/input/virtual_key.hpp>
#include <GUI/input/virtual_key_names.hpp>

#include <GUI/string/encrypted_string_t.hpp>
#include <GUI/string/xor_keys.hpp>

#include <GUI/style/colorscheme.hpp>
#include <GUI/style/element_sizes.hpp>
#include <GUI/style/rounding.hpp>
#include <GUI/style/shading.hpp>

#include <GUI/animation/blink.hpp>
#include <GUI/animation/smooth_scroll.hpp>
#include <GUI/animation/tween.hpp>

#include <GUI/debug/assert.hpp>
#include <GUI/debug/print.hpp>

#include <GUI/pages/abstract_visuals_page_t.hpp>
#include <GUI/pages/antiaim_groupbox_t.hpp>
#include <GUI/pages/configurable_player_model_t.hpp>
#include <GUI/pages/dialog_window_builder_t.hpp>
#include <GUI/pages/enemy_visuals_page_t.hpp>
#include <GUI/pages/legit_weapon_page_t.hpp>
#include <GUI/pages/rage_weapon_page_t.hpp>
#include <GUI/pages/self_visuals_page_t.hpp>
#include <GUI/pages/team_visuals_page_t.hpp>

#include <GUI/layout/build_main_gui_layout.hpp>
