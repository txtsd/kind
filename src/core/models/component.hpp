#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace kind {

struct SelectOption {
  std::string label;
  std::string value;
  std::string description;
  bool default_selected{false};

  bool operator==(const SelectOption&) const = default;
};

struct Component {
  int type{};  // 1=ActionRow, 2=Button, 3=StringSelect, etc.
  std::optional<std::string> custom_id;
  std::optional<std::string> label;
  int style{};  // Button style: 1=Primary, 2=Secondary, 3=Success, 4=Danger, 5=Link
  bool disabled{false};
  std::vector<Component> children;  // For ActionRows

  // Button-specific
  std::optional<std::string> url;  // Link buttons (style 5)
  std::optional<std::string> emoji_name;  // Unicode emoji or custom emoji name
  std::optional<uint64_t> emoji_id;       // Custom emoji snowflake (0 for Unicode)
  bool emoji_animated{false};             // Custom emoji animation flag

  // Select menu-specific
  std::optional<std::string> placeholder;
  std::vector<SelectOption> options;
  std::optional<int> min_values;
  std::optional<int> max_values;

  // Components V2 fields
  std::optional<std::string> content;  // Text Display (10) and Section (9) text children use this
  std::optional<int> accent_color;     // Container (17) accent bar color (RGB integer)
  bool spoiler{false};                 // Container (17) spoiler flag
  bool divider{true};                  // Separator (14) visible divider line
  int spacing{1};                      // Separator (14): 1=small, 2=large

  // Thumbnail (11) / media
  std::optional<std::string> media_url;      // Unfurled media item URL
  std::optional<std::string> media_proxy_url;
  std::optional<int> media_width;
  std::optional<int> media_height;

  // Section (9) accessory (stored as heap-allocated Component to avoid incomplete type)
  std::shared_ptr<Component> accessory;

  bool operator==(const Component& other) const {
    if (type != other.type) return false;
    if (custom_id != other.custom_id) return false;
    if (label != other.label) return false;
    if (style != other.style) return false;
    if (disabled != other.disabled) return false;
    if (children != other.children) return false;
    if (url != other.url) return false;
    if (emoji_name != other.emoji_name) return false;
    if (emoji_id != other.emoji_id) return false;
    if (emoji_animated != other.emoji_animated) return false;
    if (placeholder != other.placeholder) return false;
    if (options != other.options) return false;
    if (min_values != other.min_values) return false;
    if (max_values != other.max_values) return false;
    if (content != other.content) return false;
    if (accent_color != other.accent_color) return false;
    if (spoiler != other.spoiler) return false;
    if (divider != other.divider) return false;
    if (spacing != other.spacing) return false;
    if (media_url != other.media_url) return false;
    if (media_proxy_url != other.media_proxy_url) return false;
    if (media_width != other.media_width) return false;
    if (media_height != other.media_height) return false;
    // Deep compare accessory
    if (accessory && other.accessory) return *accessory == *other.accessory;
    if (accessory || other.accessory) return false;
    return true;
  }
};

} // namespace kind
