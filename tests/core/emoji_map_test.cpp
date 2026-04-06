#include "text/emoji_map.hpp"

#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

using namespace kind;

// ---------------------------------------------------------------------------
// Tier 1: Normal tests
// ---------------------------------------------------------------------------

TEST(EmojiMap, SingleKnownShortcode) {
  std::string text = ":fire:";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "🔥");
}

TEST(EmojiMap, MultipleShortcodes) {
  std::string text = ":thumbsup: nice :fire:";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "👍 nice 🔥");
}

TEST(EmojiMap, UnknownShortcodeLeftAsIs) {
  std::string text = ":definitely_not_a_real_emoji:";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, ":definitely_not_a_real_emoji:");
}

TEST(EmojiMap, PlainTextUnchanged) {
  std::string text = "hello world";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "hello world");
}

TEST(EmojiMap, EmptyStringNoOp) {
  std::string text;
  replace_emoji_shortcodes(text);
  EXPECT_TRUE(text.empty());
}

TEST(EmojiMap, ShortcodeAtStart) {
  std::string text = ":smile: hello";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "😄 hello");
}

TEST(EmojiMap, ShortcodeAtEnd) {
  std::string text = "hello :smile:";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "hello 😄");
}

TEST(EmojiMap, AdjacentShortcodes) {
  std::string text = ":fire::fire::fire:";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "🔥🔥🔥");
}

TEST(EmojiMap, ShortcodeWithUnderscores) {
  std::string text = ":rugby_football:";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "🏉");
}

TEST(EmojiMap, NumericShortcode) {
  std::string text = ":100:";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "💯");
}

TEST(EmojiMap, ReloadThenReplace) {
  reload_emoji_map();
  std::string text = ":heart:";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "❤️");
}

// ---------------------------------------------------------------------------
// Tier 2: Extensive edge cases
// ---------------------------------------------------------------------------

TEST(EmojiMapEdgeCases, SingleColon) {
  std::string text = ":";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, ":");
}

TEST(EmojiMapEdgeCases, DoubleColon) {
  std::string text = "::";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "::");
}

TEST(EmojiMapEdgeCases, TripleColon) {
  std::string text = ":::";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, ":::");
}

TEST(EmojiMapEdgeCases, ColonAtEnd) {
  std::string text = "hello:";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "hello:");
}

TEST(EmojiMapEdgeCases, ColonAtStartNoClose) {
  std::string text = ":hello";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, ":hello");
}

TEST(EmojiMapEdgeCases, SpaceInShortcodeNotReplaced) {
  std::string text = ":not an emoji:";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, ":not an emoji:");
}

TEST(EmojiMapEdgeCases, EmptyShortcode) {
  std::string text = "::rest";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "::rest");
}

TEST(EmojiMapEdgeCases, KnownMixedWithUnknown) {
  std::string text = ":fire: and :nonexistent: and :wave:";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "🔥 and :nonexistent: and 👋");
}

TEST(EmojiMapEdgeCases, ColonsInUrl) {
  std::string text = "https://example.com:8080/path";
  replace_emoji_shortcodes(text);
  // The parser may try to match "example.com" or "8080/path" as shortcodes,
  // but neither will be in the map so the text should pass through intact.
  EXPECT_EQ(text, "https://example.com:8080/path");
}

TEST(EmojiMapEdgeCases, NestedColons) {
  std::string text = ":fire:fire:";
  replace_emoji_shortcodes(text);
  // First match: :fire: -> emoji, remaining "fire:" is literal
  EXPECT_EQ(text, "🔥fire:");
}

TEST(EmojiMapEdgeCases, CustomEmojiSyntaxNotMatched) {
  // Discord custom emoji format <:name:id> should not be touched by shortcode
  // replacement since it doesn't start with a bare colon at position 0.
  std::string text = "<:custom:123456>";
  replace_emoji_shortcodes(text);
  // The < prevents the first : from being position 0 of text[i],
  // but the parser does scan mid-string. It will try :custom: which is unknown,
  // then try :123456: but there's no trailing colon. Result should be unchanged.
  EXPECT_EQ(text, "<:custom:123456>");
}

TEST(EmojiMapEdgeCases, OnlyColons) {
  std::string text = "::::::::";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "::::::::");
}

TEST(EmojiMapEdgeCases, ShortcodeInMiddleOfWord) {
  std::string text = "word:fire:word";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "word🔥word");
}

TEST(EmojiMapEdgeCases, ReloadIdempotent) {
  reload_emoji_map();
  reload_emoji_map();
  std::string text = ":fire:";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "🔥");
}

TEST(EmojiMapEdgeCases, UnicodeInSurroundingText) {
  std::string text = "日本語 :fire: テスト";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "日本語 🔥 テスト");
}

TEST(EmojiMapEdgeCases, EmojiOutputIsMultibyte) {
  // heart is a multi-byte emoji (❤️ = U+2764 + U+FE0F)
  std::string text = ":heart:";
  replace_emoji_shortcodes(text);
  EXPECT_FALSE(text.empty());
  EXPECT_NE(text, ":heart:");
  EXPECT_GT(text.size(), 1u);
}

// ---------------------------------------------------------------------------
// Tier 3: Absolutely unhinged scenarios
// ---------------------------------------------------------------------------

TEST(EmojiMapUnhinged, ThousandShortcodesInOneString) {
  std::string text;
  text.reserve(7000);
  for (int i = 0; i < 1000; ++i) {
    text += ":fire:";
  }
  replace_emoji_shortcodes(text);
  // Each :fire: (6 bytes) replaced with 🔥 (4 bytes UTF-8)
  std::string expected;
  expected.reserve(4000);
  for (int i = 0; i < 1000; ++i) {
    expected += "🔥";
  }
  EXPECT_EQ(text, expected);
}

TEST(EmojiMapUnhinged, AlternatingKnownAndUnknown) {
  std::string text;
  for (int i = 0; i < 200; ++i) {
    text += ":fire::xyzzy_not_real:";
  }
  replace_emoji_shortcodes(text);
  std::string expected;
  for (int i = 0; i < 200; ++i) {
    expected += "🔥:xyzzy_not_real:";
  }
  EXPECT_EQ(text, expected);
}

TEST(EmojiMapUnhinged, MassiveStringNoShortcodes) {
  std::string text(100000, 'a');
  std::string original = text;
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, original);
}

TEST(EmojiMapUnhinged, MassiveStringOnlyColons) {
  std::string text(10000, ':');
  replace_emoji_shortcodes(text);
  // No valid shortcode between any pair of colons (empty name),
  // so the entire string should pass through unchanged.
  EXPECT_EQ(text, std::string(10000, ':'));
}

TEST(EmojiMapUnhinged, SingleCharacterString) {
  std::string text = "x";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, "x");
}

TEST(EmojiMapUnhinged, SingleColonCharacter) {
  std::string text = ":";
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, ":");
}

TEST(EmojiMapUnhinged, ConcurrentReadsAfterLoad) {
  // Force a load first
  {
    std::string warmup = ":fire:";
    replace_emoji_shortcodes(warmup);
  }

  std::vector<std::thread> threads;
  std::atomic<int> failures{0};

  for (int t = 0; t < 8; ++t) {
    threads.emplace_back([&failures]() {
      for (int i = 0; i < 500; ++i) {
        std::string text = ":thumbsup: :wave:";
        replace_emoji_shortcodes(text);
        if (text != "👍 👋") {
          ++failures;
        }
      }
    });
  }
  for (auto& th : threads) {
    th.join();
  }
  EXPECT_EQ(failures.load(), 0);
}

TEST(EmojiMapUnhinged, ReloadWhileReading) {
  // Hammer reload_emoji_map while other threads are replacing.
  // This should not crash or produce garbage.
  std::atomic<bool> stop{false};
  std::atomic<int> crashes{0};

  std::thread reloader([&stop]() {
    while (!stop.load()) {
      reload_emoji_map();
    }
  });

  std::vector<std::thread> readers;
  for (int t = 0; t < 4; ++t) {
    readers.emplace_back([&stop, &crashes]() {
      while (!stop.load()) {
        std::string text = ":fire:";
        replace_emoji_shortcodes(text);
        // Result must be either the emoji or the original shortcode
        // (if reload cleared the map mid-operation).
        if (text != "🔥" && text != ":fire:") {
          ++crashes;
        }
      }
    });
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  stop.store(true);

  reloader.join();
  for (auto& th : readers) {
    th.join();
  }
  EXPECT_EQ(crashes.load(), 0);
}

TEST(EmojiMapUnhinged, ShortcodeNameIsEntireAlphabet) {
  // A very long but valid-looking shortcode name that won't match anything
  std::string text = ":abcdefghijklmnopqrstuvwxyz_0123456789_ABCDEFGHIJKLMNOPQRSTUVWXYZ:";
  std::string original = text;
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, original);
}

TEST(EmojiMapUnhinged, BinaryGarbageBetweenColons) {
  std::string text = ":";
  text += '\0';
  text += '\x01';
  text += '\xFF';
  text += ':';
  // Contains null bytes and high bytes, but no spaces, so parser will attempt
  // a lookup. It won't match anything, so should pass through.
  std::string original = text;
  replace_emoji_shortcodes(text);
  EXPECT_EQ(text, original);
}

TEST(EmojiMapUnhinged, EveryShortcodeInOneMessage) {
  // Build a string with a handful of known shortcodes separated by spaces,
  // then verify each one was replaced (none left as :name:).
  std::vector<std::string> codes = {
    "fire", "heart", "smile", "thumbsup", "wave",
    "100", "eggplant", "soccer", "basketball", "baseball"
  };
  std::string text;
  for (const auto& c : codes) {
    text += ":" + c + ": ";
  }
  replace_emoji_shortcodes(text);
  // None of the original shortcode markers should remain
  for (const auto& c : codes) {
    EXPECT_EQ(text.find(":" + c + ":"), std::string::npos)
      << "Shortcode :" << c << ": was not replaced";
  }
}
