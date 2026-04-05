#pragma once

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

  // Select menu-specific
  std::optional<std::string> placeholder;
  std::vector<SelectOption> options;
  std::optional<int> min_values;
  std::optional<int> max_values;

  bool operator==(const Component&) const = default;
};

} // namespace kind
