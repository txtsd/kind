#pragma once

#include <string>

namespace kind {

// Replace emoji shortcodes like :eggplant: with their Unicode equivalents.
// Only replaces known shortcodes. Unknown shortcodes are left as-is.
void replace_emoji_shortcodes(std::string& text);

} // namespace kind
