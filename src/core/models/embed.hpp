#pragma once
#include <optional>
#include <string>
namespace kind {
struct Embed {
  std::optional<std::string> title;
  std::optional<std::string> description;
  std::optional<std::string> url;
  std::optional<int> color;
};
}
