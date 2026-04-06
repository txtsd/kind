#pragma once

#include <string>

namespace kind {

// Replace emoji shortcodes like :eggplant: with their Unicode equivalents.
// Loads shortcode data from the bundled emoji.json Qt resource on first use.
// Unknown shortcodes are left as-is.
void replace_emoji_shortcodes(std::string& text);

// Reload the emoji map from the bundled resource.
void reload_emoji_map();

} // namespace kind
