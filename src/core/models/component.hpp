#pragma once

#include <optional>
#include <string>
#include <vector>

namespace kind {

struct Component {
  int type{};  // 1=ActionRow, 2=Button, 3=StringSelect, etc.
  std::optional<std::string> custom_id;
  std::optional<std::string> label;
  int style{};  // Button style: 1=Primary, 2=Secondary, 3=Success, 4=Danger, 5=Link
  bool disabled{false};
  std::vector<Component> children;  // For ActionRows

  bool operator==(const Component&) const = default;
};

} // namespace kind
