#include "text/markdown_parser.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <string_view>

namespace kind::markdown {

namespace {

// Flush accumulated plain text as a TextSpan into blocks
void flush_text(std::string& accum, uint8_t style,
                std::vector<std::variant<TextSpan, CodeBlock>>& blocks) {
  if (accum.empty()) return;
  TextSpan span;
  span.text = std::move(accum);
  span.style = style;
  blocks.push_back(std::move(span));
  accum.clear();
}

// Try to parse a Snowflake from a string_view of digits
std::optional<Snowflake> parse_snowflake(std::string_view sv) {
  if (sv.empty()) return std::nullopt;
  Snowflake val{};
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
  if (ec != std::errc{} || ptr != sv.data() + sv.size()) return std::nullopt;
  return val;
}

// Check if every character in the view is a digit
bool all_digits(std::string_view sv) {
  return !sv.empty() &&
         std::all_of(sv.begin(), sv.end(), [](char c) { return c >= '0' && c <= '9'; });
}

// Try to parse an angle-bracket construct: mentions or custom emoji.
// Returns the number of characters consumed (0 if no match).
size_t try_parse_angle_bracket(const std::string& content, size_t pos,
                               std::vector<std::variant<TextSpan, CodeBlock>>& blocks) {
  if (pos >= content.size() || content[pos] != '<') return 0;

  auto close = content.find('>', pos + 1);
  if (close == std::string::npos) return 0;

  std::string_view inner(content.data() + pos + 1, close - pos - 1);

  // User mention: <@123> or <@!123>
  if (inner.size() >= 2 && inner[0] == '@' && inner[1] != '&') {
    std::string_view id_part = inner.substr(1);
    if (!id_part.empty() && id_part[0] == '!') id_part = id_part.substr(1);
    if (all_digits(id_part)) {
      auto id = parse_snowflake(id_part);
      if (id) {
        TextSpan span;
        span.text = std::string(content.data() + pos, close - pos + 1);
        span.mention_user_id = *id;
        blocks.push_back(std::move(span));
        return close - pos + 1;
      }
    }
  }

  // Channel mention: <#123>
  if (inner.size() >= 2 && inner[0] == '#') {
    std::string_view id_part = inner.substr(1);
    if (all_digits(id_part)) {
      auto id = parse_snowflake(id_part);
      if (id) {
        TextSpan span;
        span.text = std::string(content.data() + pos, close - pos + 1);
        span.mention_channel_id = *id;
        blocks.push_back(std::move(span));
        return close - pos + 1;
      }
    }
  }

  // Role mention: <@&123>
  if (inner.size() >= 3 && inner[0] == '@' && inner[1] == '&') {
    std::string_view id_part = inner.substr(2);
    if (all_digits(id_part)) {
      auto id = parse_snowflake(id_part);
      if (id) {
        TextSpan span;
        span.text = std::string(content.data() + pos, close - pos + 1);
        span.mention_role_id = *id;
        blocks.push_back(std::move(span));
        return close - pos + 1;
      }
    }
  }

  // Custom emoji: <:name:id> or <a:name:id>
  {
    bool animated = false;
    std::string_view rest = inner;
    if (!rest.empty() && rest[0] == 'a') {
      animated = true;
      rest = rest.substr(1);
    }
    if (rest.size() >= 3 && rest[0] == ':') {
      auto second_colon = rest.find(':', 1);
      if (second_colon != std::string_view::npos && second_colon + 1 < rest.size()) {
        std::string_view name = rest.substr(1, second_colon - 1);
        std::string_view id_part = rest.substr(second_colon + 1);
        if (!name.empty() && all_digits(id_part)) {
          auto id = parse_snowflake(id_part);
          if (id) {
            TextSpan span;
            span.text = std::string(content.data() + pos, close - pos + 1);
            span.custom_emoji_id = *id;
            span.custom_emoji_name = std::string(name);
            span.animated_emoji = animated;
            blocks.push_back(std::move(span));
            return close - pos + 1;
          }
        }
      }
    }
  }

  // Suppressed-embed URL: <https://example.com>
  if (inner.starts_with("https://") || inner.starts_with("http://")) {
    TextSpan span;
    span.text = std::string(inner);
    span.link_url = std::string(inner);
    blocks.push_back(std::move(span));
    return close - pos + 1;
  }

  return 0;
}

// Try to match a masked link [text](url) at pos
size_t try_parse_masked_link(const std::string& content, size_t pos,
                             std::vector<std::variant<TextSpan, CodeBlock>>& blocks) {
  if (pos >= content.size() || content[pos] != '[') return 0;

  auto bracket_close = content.find(']', pos + 1);
  if (bracket_close == std::string::npos) return 0;
  if (bracket_close + 1 >= content.size() || content[bracket_close + 1] != '(') return 0;

  auto paren_close = content.find(')', bracket_close + 2);
  if (paren_close == std::string::npos) return 0;

  std::string text(content.data() + pos + 1, bracket_close - pos - 1);
  std::string url(content.data() + bracket_close + 2, paren_close - bracket_close - 2);

  TextSpan span;
  span.text = std::move(text);
  span.link_url = std::move(url);
  blocks.push_back(std::move(span));
  return paren_close - pos + 1;
}

// Parse inline formatting within a text segment (no code blocks).
// The style parameter is the inherited bitmask from outer formatting.
void parse_inline(const std::string& content, size_t start, size_t end, uint8_t style,
                  std::vector<std::variant<TextSpan, CodeBlock>>& blocks) {
  std::string accum;
  size_t i = start;

  while (i < end) {
    // Inline code: highest priority within inline content, no nesting
    if (content[i] == '`') {
      auto close = content.find('`', i + 1);
      if (close != std::string::npos && close <= end) {
        flush_text(accum, style, blocks);
        TextSpan span;
        span.text = std::string(content.data() + i + 1, close - i - 1);
        span.style = TextSpan::InlineCode;
        blocks.push_back(std::move(span));
        i = close + 1;
        continue;
      }
    }

    // Angle bracket constructs (mentions, emoji)
    if (content[i] == '<') {
      flush_text(accum, style, blocks);
      size_t consumed = try_parse_angle_bracket(content, i, blocks);
      if (consumed > 0) {
        i += consumed;
        continue;
      }
      // Not a recognized construct, treat as literal
      accum += content[i];
      ++i;
      continue;
    }

    // Masked links
    if (content[i] == '[') {
      flush_text(accum, style, blocks);
      size_t consumed = try_parse_masked_link(content, i, blocks);
      if (consumed > 0) {
        i += consumed;
        continue;
      }
      accum += content[i];
      ++i;
      continue;
    }

    // Auto-linked URLs
    if (content[i] == 'h') {
      std::string_view remaining(content.data() + i, end - i);
      if (remaining.starts_with("https://") || remaining.starts_with("http://")) {
        flush_text(accum, style, blocks);
        // Find end of URL within our boundary
        size_t url_end = i;
        while (url_end < end && content[url_end] != ' ' && content[url_end] != '\t' &&
               content[url_end] != '\n' && content[url_end] != '\r' &&
               content[url_end] != '>') {
          ++url_end;
        }
        std::string url(content.data() + i, url_end - i);
        TextSpan span;
        span.text = url;
        span.style = style;
        span.link_url = std::move(url);
        blocks.push_back(std::move(span));
        i = url_end;
        continue;
      }
    }

    // Spoiler: ||text||
    if (i + 1 < end && content[i] == '|' && content[i + 1] == '|') {
      auto close = content.find("||", i + 2);
      if (close != std::string::npos && close + 2 <= end) {
        flush_text(accum, style, blocks);
        parse_inline(content, i + 2, close, style | TextSpan::Spoiler, blocks);
        i = close + 2;
        continue;
      }
    }

    // Asterisk-based formatting: ***, **, *
    if (content[i] == '*') {
      // Bold italic: ***text***
      if (i + 2 < end && content[i + 1] == '*' && content[i + 2] == '*') {
        auto close = content.find("***", i + 3);
        if (close != std::string::npos && close + 3 <= end) {
          flush_text(accum, style, blocks);
          parse_inline(content, i + 3, close,
                       style | TextSpan::Bold | TextSpan::Italic, blocks);
          i = close + 3;
          continue;
        }
        // Unclosed ***: treat all three as literal
        accum += "***";
        i += 3;
        continue;
      }

      // Bold: **text**
      if (i + 1 < end && content[i + 1] == '*') {
        auto close = content.find("**", i + 2);
        if (close != std::string::npos && close + 2 <= end) {
          flush_text(accum, style, blocks);
          parse_inline(content, i + 2, close, style | TextSpan::Bold, blocks);
          i = close + 2;
          continue;
        }
        // Unclosed **: treat both as literal
        accum += "**";
        i += 2;
        continue;
      }

      // Italic: *text*
      {
        auto close = content.find('*', i + 1);
        if (close != std::string::npos && close <= end) {
          flush_text(accum, style, blocks);
          parse_inline(content, i + 1, close, style | TextSpan::Italic, blocks);
          i = close + 1;
          continue;
        }
      }

      // Unclosed single *: treat as literal
      accum += '*';
      ++i;
      continue;
    }

    // Underline: __text__
    if (i + 1 < end && content[i] == '_' && content[i + 1] == '_') {
      auto close = content.find("__", i + 2);
      if (close != std::string::npos && close + 2 <= end) {
        flush_text(accum, style, blocks);
        parse_inline(content, i + 2, close, style | TextSpan::Underline, blocks);
        i = close + 2;
        continue;
      }
    }

    // Strikethrough: ~~text~~
    if (i + 1 < end && content[i] == '~' && content[i + 1] == '~') {
      auto close = content.find("~~", i + 2);
      if (close != std::string::npos && close + 2 <= end) {
        flush_text(accum, style, blocks);
        parse_inline(content, i + 2, close, style | TextSpan::Strikethrough, blocks);
        i = close + 2;
        continue;
      }
    }

    accum += content[i];
    ++i;
  }

  flush_text(accum, style, blocks);
}

} // anonymous namespace

ParsedContent parse(const std::string& content) {
  ParsedContent result;
  if (content.empty()) return result;

  std::string working = content;

  // Handle block quotes: strip leading "> " or ">>> "
  if (working.starts_with(">>> ")) {
    result.has_block_quote = true;
    working = working.substr(4);
  } else if (working.starts_with("> ")) {
    result.has_block_quote = true;
    working = working.substr(2);
  }

  // Handle headings: "# ", "## ", "### " at start
  if (working.starts_with("### ")) {
    result.heading_level = 3;
    working = working.substr(4);
  } else if (working.starts_with("## ")) {
    result.heading_level = 2;
    working = working.substr(3);
  } else if (working.starts_with("# ")) {
    result.heading_level = 1;
    working = working.substr(2);
  }

  // First pass: split on code blocks (``` delimiters)
  std::string fence = "```";
  size_t pos = 0;

  while (pos < working.size()) {
    auto fence_start = working.find(fence, pos);

    if (fence_start == std::string::npos) {
      // No more code blocks, parse the rest as inline
      parse_inline(working, pos, working.size(), TextSpan::Normal, result.blocks);
      break;
    }

    // Parse any inline content before the code block
    if (fence_start > pos) {
      parse_inline(working, pos, fence_start, TextSpan::Normal, result.blocks);
    }

    // Find end of opening fence line (for language tag)
    size_t lang_start = fence_start + 3;
    size_t line_end = working.find('\n', lang_start);
    if (line_end == std::string::npos) {
      // Unclosed code block, treat rest as literal text
      parse_inline(working, fence_start, working.size(), TextSpan::Normal, result.blocks);
      break;
    }

    std::string language(working.data() + lang_start, line_end - lang_start);
    // Trim whitespace from language
    while (!language.empty() && (language.back() == ' ' || language.back() == '\r')) {
      language.pop_back();
    }

    // Find closing fence
    size_t code_start = line_end + 1;
    auto fence_end = working.find(fence, code_start);
    if (fence_end == std::string::npos) {
      // Unclosed code block, treat as literal
      parse_inline(working, fence_start, working.size(), TextSpan::Normal, result.blocks);
      break;
    }

    std::string code(working.data() + code_start, fence_end - code_start);

    CodeBlock block;
    block.language = std::move(language);
    block.code = std::move(code);
    result.blocks.push_back(std::move(block));

    pos = fence_end + 3;
    // Skip optional newline after closing fence
    if (pos < working.size() && working[pos] == '\n') {
      ++pos;
    }
  }

  return result;
}

} // namespace kind::markdown
