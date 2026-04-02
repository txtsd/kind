#include "text/markdown_parser.hpp"

#include <gtest/gtest.h>

using namespace kind;

// Helper to get the first TextSpan from parsed content
static TextSpan first_span(const ParsedContent& pc) {
  return std::get<TextSpan>(pc.blocks[0]);
}

TEST(MarkdownParser, PlainText) {
  auto result = markdown::parse("hello world");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  EXPECT_EQ(span.text, "hello world");
  EXPECT_EQ(span.style, TextSpan::Normal);
}

TEST(MarkdownParser, Bold) {
  auto result = markdown::parse("**bold**");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  EXPECT_EQ(span.text, "bold");
  EXPECT_TRUE(span.style & TextSpan::Bold);
}

TEST(MarkdownParser, Italic) {
  auto result = markdown::parse("*italic*");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  EXPECT_EQ(span.text, "italic");
  EXPECT_TRUE(span.style & TextSpan::Italic);
}

TEST(MarkdownParser, BoldItalic) {
  auto result = markdown::parse("***bold italic***");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  EXPECT_EQ(span.text, "bold italic");
  EXPECT_TRUE(span.style & TextSpan::Bold);
  EXPECT_TRUE(span.style & TextSpan::Italic);
}

TEST(MarkdownParser, Strikethrough) {
  auto result = markdown::parse("~~struck~~");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  EXPECT_EQ(span.text, "struck");
  EXPECT_TRUE(span.style & TextSpan::Strikethrough);
}

TEST(MarkdownParser, Underline) {
  auto result = markdown::parse("__underlined__");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  EXPECT_EQ(span.text, "underlined");
  EXPECT_TRUE(span.style & TextSpan::Underline);
}

TEST(MarkdownParser, InlineCode) {
  auto result = markdown::parse("`code`");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  EXPECT_EQ(span.text, "code");
  EXPECT_TRUE(span.style & TextSpan::InlineCode);
}

TEST(MarkdownParser, Spoiler) {
  auto result = markdown::parse("||spoiler||");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  EXPECT_EQ(span.text, "spoiler");
  EXPECT_TRUE(span.style & TextSpan::Spoiler);
}

TEST(MarkdownParser, CodeBlock) {
  auto result = markdown::parse("```cpp\nint x = 1;\n```");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto& block = std::get<CodeBlock>(result.blocks[0]);
  EXPECT_EQ(block.language, "cpp");
  EXPECT_EQ(block.code, "int x = 1;\n");
}

TEST(MarkdownParser, CodeBlockNoLanguage) {
  auto result = markdown::parse("```\nsome code\n```");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto& block = std::get<CodeBlock>(result.blocks[0]);
  EXPECT_TRUE(block.language.empty());
  EXPECT_EQ(block.code, "some code\n");
}

TEST(MarkdownParser, MixedTextAndBold) {
  auto result = markdown::parse("hello **world**");
  ASSERT_EQ(result.blocks.size(), 2u);
  auto span1 = std::get<TextSpan>(result.blocks[0]);
  EXPECT_EQ(span1.text, "hello ");
  EXPECT_EQ(span1.style, TextSpan::Normal);
  auto span2 = std::get<TextSpan>(result.blocks[1]);
  EXPECT_EQ(span2.text, "world");
  EXPECT_TRUE(span2.style & TextSpan::Bold);
}

TEST(MarkdownParser, MaskedLink) {
  auto result = markdown::parse("[click here](https://example.com)");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  EXPECT_EQ(span.text, "click here");
  ASSERT_TRUE(span.link_url.has_value());
  EXPECT_EQ(*span.link_url, "https://example.com");
}

TEST(MarkdownParser, MaskedLinkWithSurroundingText) {
  auto result = markdown::parse("Check out [this link](https://example.com) for details");
  ASSERT_EQ(result.blocks.size(), 3u);
  auto span1 = std::get<TextSpan>(result.blocks[0]);
  EXPECT_EQ(span1.text, "Check out ");
  EXPECT_FALSE(span1.link_url.has_value());
  auto span2 = std::get<TextSpan>(result.blocks[1]);
  EXPECT_EQ(span2.text, "this link");
  ASSERT_TRUE(span2.link_url.has_value());
  EXPECT_EQ(*span2.link_url, "https://example.com");
  auto span3 = std::get<TextSpan>(result.blocks[2]);
  EXPECT_EQ(span3.text, " for details");
}

TEST(MarkdownParser, AngleBracketSuppressedUrl) {
  auto result = markdown::parse("<https://example.com>");
  ASSERT_GE(result.blocks.size(), 1u);
  auto span = std::get<TextSpan>(result.blocks[0]);
  ASSERT_TRUE(span.link_url.has_value());
  EXPECT_EQ(*span.link_url, "https://example.com");
  EXPECT_EQ(span.text.find('<'), std::string::npos);
  EXPECT_EQ(span.text.find('>'), std::string::npos);
}

TEST(MarkdownParser, AngleBracketUrlWithTrailingText) {
  auto result = markdown::parse("<https://example.com>\n\nSome text after");
  ASSERT_GE(result.blocks.size(), 2u);
  auto span1 = std::get<TextSpan>(result.blocks[0]);
  ASSERT_TRUE(span1.link_url.has_value());
  EXPECT_EQ(*span1.link_url, "https://example.com");
  auto span2 = std::get<TextSpan>(result.blocks[1]);
  EXPECT_TRUE(span2.text.find("Some text after") != std::string::npos);
}

TEST(MarkdownParser, UserMention) {
  auto result = markdown::parse("<@123456>");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  ASSERT_TRUE(span.mention_user_id.has_value());
  EXPECT_EQ(*span.mention_user_id, 123456u);
}

TEST(MarkdownParser, UserMentionWithExclamation) {
  auto result = markdown::parse("<@!789>");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  ASSERT_TRUE(span.mention_user_id.has_value());
  EXPECT_EQ(*span.mention_user_id, 789u);
}

TEST(MarkdownParser, ChannelMention) {
  auto result = markdown::parse("<#456>");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  ASSERT_TRUE(span.mention_channel_id.has_value());
  EXPECT_EQ(*span.mention_channel_id, 456u);
}

TEST(MarkdownParser, RoleMention) {
  auto result = markdown::parse("<@&789>");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  ASSERT_TRUE(span.mention_role_id.has_value());
  EXPECT_EQ(*span.mention_role_id, 789u);
}

TEST(MarkdownParser, CustomEmoji) {
  auto result = markdown::parse("<:smile:123>");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  ASSERT_TRUE(span.custom_emoji_id.has_value());
  EXPECT_EQ(*span.custom_emoji_id, 123u);
  ASSERT_TRUE(span.custom_emoji_name.has_value());
  EXPECT_EQ(*span.custom_emoji_name, "smile");
  EXPECT_FALSE(span.animated_emoji);
}

TEST(MarkdownParser, AnimatedCustomEmoji) {
  auto result = markdown::parse("<a:wave:456>");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  ASSERT_TRUE(span.custom_emoji_id.has_value());
  EXPECT_EQ(*span.custom_emoji_id, 456u);
  EXPECT_EQ(*span.custom_emoji_name, "wave");
  EXPECT_TRUE(span.animated_emoji);
}

TEST(MarkdownParser, AtEveryoneStandalone) {
  auto result = markdown::parse("@everyone");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  EXPECT_EQ(span.text, "@everyone");
  EXPECT_EQ(span.style, TextSpan::Normal);
  EXPECT_FALSE(span.mention_user_id.has_value());
}

TEST(MarkdownParser, AtHereStandalone) {
  auto result = markdown::parse("@here");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  EXPECT_EQ(span.text, "@here");
  EXPECT_EQ(span.style, TextSpan::Normal);
}

TEST(MarkdownParser, AtEveryoneWithSurroundingText) {
  auto result = markdown::parse("hey @everyone check this");
  ASSERT_EQ(result.blocks.size(), 3u);
  auto span1 = std::get<TextSpan>(result.blocks[0]);
  EXPECT_EQ(span1.text, "hey ");
  auto span2 = std::get<TextSpan>(result.blocks[1]);
  EXPECT_EQ(span2.text, "@everyone");
  auto span3 = std::get<TextSpan>(result.blocks[2]);
  EXPECT_EQ(span3.text, " check this");
}

TEST(MarkdownParser, AtHereWithSurroundingText) {
  auto result = markdown::parse("hey @here!");
  ASSERT_EQ(result.blocks.size(), 3u);
  auto span1 = std::get<TextSpan>(result.blocks[0]);
  EXPECT_EQ(span1.text, "hey ");
  auto span2 = std::get<TextSpan>(result.blocks[1]);
  EXPECT_EQ(span2.text, "@here");
  auto span3 = std::get<TextSpan>(result.blocks[2]);
  EXPECT_EQ(span3.text, "!");
}

TEST(MarkdownParser, AtEveryoneInBold) {
  auto result = markdown::parse("**@everyone**");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  EXPECT_EQ(span.text, "@everyone");
  EXPECT_TRUE(span.style & TextSpan::Bold);
}

TEST(MarkdownParser, AtSignAloneNotSpecial) {
  auto result = markdown::parse("@somethingelse");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  EXPECT_EQ(span.text, "@somethingelse");
}

TEST(MarkdownParser, AutoLinkedUrl) {
  auto result = markdown::parse("visit https://example.com today");
  ASSERT_GE(result.blocks.size(), 3u);
  // Middle span should be the URL with link_url set
  bool found_link = false;
  for (const auto& block : result.blocks) {
    if (auto* span = std::get_if<TextSpan>(&block)) {
      if (span->link_url.has_value() && *span->link_url == "https://example.com") {
        found_link = true;
        EXPECT_EQ(span->text, "https://example.com");
      }
    }
  }
  EXPECT_TRUE(found_link);
}

TEST(MarkdownParser, BlockQuote) {
  auto result = markdown::parse("> quoted text");
  EXPECT_TRUE(result.has_block_quote);
  ASSERT_GE(result.blocks.size(), 1u);
}

TEST(MarkdownParser, Heading1) {
  auto result = markdown::parse("# Heading");
  EXPECT_EQ(result.heading_level, 1);
}

TEST(MarkdownParser, Heading2) {
  auto result = markdown::parse("## Heading");
  EXPECT_EQ(result.heading_level, 2);
}

TEST(MarkdownParser, Heading3) {
  auto result = markdown::parse("### Heading");
  EXPECT_EQ(result.heading_level, 3);
}

TEST(MarkdownParser, UnclosedBoldTreatedAsLiteral) {
  auto result = markdown::parse("**unclosed");
  ASSERT_EQ(result.blocks.size(), 1u);
  auto span = first_span(result);
  EXPECT_EQ(span.text, "**unclosed");
  EXPECT_EQ(span.style, TextSpan::Normal);
}

TEST(MarkdownParser, EmptyContent) {
  auto result = markdown::parse("");
  EXPECT_TRUE(result.blocks.empty());
}

TEST(MarkdownParser, CodeBlockBeforeText) {
  auto result = markdown::parse("```\ncode\n```\nafter");
  ASSERT_GE(result.blocks.size(), 2u);
  EXPECT_TRUE(std::holds_alternative<CodeBlock>(result.blocks[0]));
  EXPECT_TRUE(std::holds_alternative<TextSpan>(result.blocks[1]));
}

TEST(MarkdownParser, MultipleCodeBlocksWithTextBetween) {
  auto result = markdown::parse("```lua\ncode1\n```\ntext between\n```lua\ncode2\n```");
  ASSERT_EQ(result.blocks.size(), 3u);
  auto& cb1 = std::get<CodeBlock>(result.blocks[0]);
  EXPECT_EQ(cb1.language, "lua");
  EXPECT_EQ(cb1.code, "code1\n");
  auto& span = std::get<TextSpan>(result.blocks[1]);
  EXPECT_EQ(span.text, "text between\n");
  auto& cb2 = std::get<CodeBlock>(result.blocks[2]);
  EXPECT_EQ(cb2.language, "lua");
  EXPECT_EQ(cb2.code, "code2\n");
}
