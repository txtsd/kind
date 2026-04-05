#include "models/attachment.hpp"
#include "models/channel.hpp"
#include "models/embed.hpp"
#include "models/guild.hpp"
#include "models/message.hpp"
#include "models/snowflake.hpp"
#include "models/user.hpp"
#include "utils/nonce.hpp"

#include <QThread>
#include <climits>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Tier 1: Normal usage
// ---------------------------------------------------------------------------

TEST(Snowflake, DefaultIsZero) {
  kind::Snowflake s{};
  EXPECT_EQ(s, 0u);
}

TEST(User, DefaultConstruction) {
  kind::User u;
  EXPECT_EQ(u.id, 0u);
  EXPECT_TRUE(u.username.empty());
  EXPECT_TRUE(u.discriminator.empty());
  EXPECT_TRUE(u.avatar_hash.empty());
  EXPECT_FALSE(u.bot);
}

TEST(Attachment, DefaultConstruction) {
  kind::Attachment a;
  EXPECT_EQ(a.id, 0u);
  EXPECT_TRUE(a.filename.empty());
  EXPECT_TRUE(a.url.empty());
  EXPECT_EQ(a.size, 0u);
  EXPECT_FALSE(a.width.has_value());
  EXPECT_FALSE(a.height.has_value());
}

TEST(Embed, DefaultConstruction) {
  kind::Embed e;
  EXPECT_FALSE(e.title.has_value());
  EXPECT_FALSE(e.description.has_value());
  EXPECT_FALSE(e.url.has_value());
  EXPECT_FALSE(e.color.has_value());
}

TEST(Channel, DefaultConstruction) {
  kind::Channel c;
  EXPECT_EQ(c.id, 0u);
  EXPECT_EQ(c.guild_id, 0u);
  EXPECT_TRUE(c.name.empty());
  EXPECT_EQ(c.type, 0);
  EXPECT_EQ(c.position, 0);
  EXPECT_FALSE(c.parent_id.has_value());
}

TEST(Guild, DefaultConstruction) {
  kind::Guild g;
  EXPECT_EQ(g.id, 0u);
  EXPECT_TRUE(g.name.empty());
  EXPECT_TRUE(g.icon_hash.empty());
  EXPECT_EQ(g.owner_id, 0u);
  EXPECT_TRUE(g.channels.empty());
}

TEST(Message, DefaultConstruction) {
  kind::Message m;
  EXPECT_EQ(m.id, 0u);
  EXPECT_EQ(m.channel_id, 0u);
  EXPECT_EQ(m.author.id, 0u);
  EXPECT_TRUE(m.content.empty());
  EXPECT_TRUE(m.timestamp.empty());
  EXPECT_FALSE(m.edited_timestamp.has_value());
  EXPECT_FALSE(m.pinned);
  EXPECT_TRUE(m.attachments.empty());
  EXPECT_TRUE(m.embeds.empty());
}

TEST(User, CopySemantics) {
  kind::User a;
  a.id = 42;
  a.username = "alice";
  kind::User b = a;
  EXPECT_EQ(b.id, 42u);
  EXPECT_EQ(b.username, "alice");
}

TEST(User, MoveSemantics) {
  kind::User a;
  a.id = 99;
  a.username = "bob";
  kind::User b = std::move(a);
  EXPECT_EQ(b.id, 99u);
  EXPECT_EQ(b.username, "bob");
}

TEST(Message, CopySemantics) {
  kind::Message m;
  m.id = 1;
  m.content = "hello";
  m.attachments.push_back(kind::Attachment{
      .id = 10, .filename = "pic.png", .url = {}, .size = 0, .width = std::nullopt, .height = std::nullopt});
  kind::Message m2 = m;
  EXPECT_EQ(m2.id, 1u);
  EXPECT_EQ(m2.content, "hello");
  ASSERT_EQ(m2.attachments.size(), 1u);
  EXPECT_EQ(m2.attachments[0].filename, "pic.png");
}

TEST(Message, MoveSemantics) {
  kind::Message m;
  m.id = 2;
  m.content = "world";
  kind::Message m2 = std::move(m);
  EXPECT_EQ(m2.id, 2u);
  EXPECT_EQ(m2.content, "world");
}

// ---------------------------------------------------------------------------
// Equality comparisons
// ---------------------------------------------------------------------------

TEST(User, EqualityEqual) {
  kind::User a;
  a.id = 42;
  a.username = "alice";
  a.discriminator = "1234";
  a.avatar_hash = "abc";
  a.bot = false;

  kind::User b = a;
  EXPECT_EQ(a, b);
}

TEST(User, EqualityNotEqual) {
  kind::User a;
  a.id = 42;
  a.username = "alice";

  kind::User b;
  b.id = 43;
  b.username = "bob";

  EXPECT_NE(a, b);
}

TEST(Message, EqualityEqual) {
  kind::Message a;
  a.id = 1;
  a.channel_id = 10;
  a.content = "hello";
  a.author.id = 42;
  a.author.username = "alice";

  kind::Message b = a;
  EXPECT_EQ(a, b);
}

TEST(Message, EqualityNotEqual) {
  kind::Message a;
  a.id = 1;
  a.content = "hello";

  kind::Message b;
  b.id = 2;
  b.content = "world";

  EXPECT_NE(a, b);
}

// ---------------------------------------------------------------------------
// Self-referential / default author
// ---------------------------------------------------------------------------

TEST(Message, DefaultAuthorIdIsZero) {
  kind::Message m;
  m.id = 100;
  m.content = "ghost message";
  // author is default-constructed, so its id should be 0
  EXPECT_EQ(m.author.id, 0u);
  EXPECT_TRUE(m.author.username.empty());
}

// ---------------------------------------------------------------------------
// Tier 2: Extensive edge cases
// ---------------------------------------------------------------------------

TEST(Snowflake, MaxValue) {
  kind::Snowflake s = UINT64_MAX;
  EXPECT_EQ(s, UINT64_MAX);
}

TEST(User, EmptyStrings) {
  kind::User u;
  u.username = "";
  u.discriminator = "";
  u.avatar_hash = "";
  EXPECT_TRUE(u.username.empty());
  EXPECT_TRUE(u.discriminator.empty());
  EXPECT_TRUE(u.avatar_hash.empty());
}

TEST(Attachment, OptionalFieldsDefault) {
  kind::Attachment a;
  EXPECT_EQ(a.width, std::nullopt);
  EXPECT_EQ(a.height, std::nullopt);
}

TEST(Attachment, OptionalFieldsSet) {
  kind::Attachment a;
  a.width = 1920;
  a.height = 1080;
  ASSERT_TRUE(a.width.has_value());
  ASSERT_TRUE(a.height.has_value());
  EXPECT_EQ(*a.width, 1920);
  EXPECT_EQ(*a.height, 1080);
}

TEST(Embed, OptionalFieldsDefault) {
  kind::Embed e;
  EXPECT_EQ(e.title, std::nullopt);
  EXPECT_EQ(e.description, std::nullopt);
  EXPECT_EQ(e.url, std::nullopt);
  EXPECT_EQ(e.color, std::nullopt);
}

TEST(Embed, OptionalFieldsSet) {
  kind::Embed e;
  e.title = "Hello";
  e.description = "World";
  e.url = "https://example.com";
  e.color = 0xFF0000;
  EXPECT_EQ(*e.title, "Hello");
  EXPECT_EQ(*e.description, "World");
  EXPECT_EQ(*e.url, "https://example.com");
  EXPECT_EQ(*e.color, 0xFF0000);
}

TEST(Channel, OptionalParentIdDefault) {
  kind::Channel c;
  EXPECT_EQ(c.parent_id, std::nullopt);
}

TEST(Channel, OptionalParentIdSet) {
  kind::Channel c;
  c.parent_id = 12345u;
  ASSERT_TRUE(c.parent_id.has_value());
  EXPECT_EQ(*c.parent_id, 12345u);
}

TEST(Message, EditedTimestampDefault) {
  kind::Message m;
  EXPECT_EQ(m.edited_timestamp, std::nullopt);
}

TEST(Message, EditedTimestampSet) {
  kind::Message m;
  m.edited_timestamp = "2025-01-01T00:00:00Z";
  ASSERT_TRUE(m.edited_timestamp.has_value());
  EXPECT_EQ(*m.edited_timestamp, "2025-01-01T00:00:00Z");
}

// ---------------------------------------------------------------------------
// Tier 3: Unhinged / adversarial
// ---------------------------------------------------------------------------

TEST(Message, TenThousandAttachments) {
  kind::Message m;
  for (int i = 0; i < 10000; ++i) {
    m.attachments.push_back(kind::Attachment{
        .id = static_cast<kind::Snowflake>(i),
        .filename = "file_" + std::to_string(i) + ".bin",
        .url = {},
        .size = 0,
        .width = std::nullopt,
        .height = std::nullopt,
    });
  }
  ASSERT_EQ(m.attachments.size(), 10000u);
  EXPECT_EQ(m.attachments[9999].filename, "file_9999.bin");
}

TEST(User, FourMegabyteUsername) {
  // 4 MB of a repeated emoji sequence (4 bytes per codepoint)
  const std::string emoji = "\xF0\x9F\x98\x80"; // U+1F600
  const std::size_t target = 4u * 1024u * 1024u;
  std::string big;
  big.reserve(target);
  while (big.size() + emoji.size() <= target) {
    big += emoji;
  }
  kind::User u;
  u.username = std::move(big);
  EXPECT_GE(u.username.size(), target - emoji.size());
}

TEST(Snowflake, ZeroDoesNotCrash) {
  kind::Snowflake s = 0;
  EXPECT_EQ(s, 0u);
  // Use it as an id in a model
  kind::User u;
  u.id = s;
  EXPECT_EQ(u.id, 0u);
}

TEST(Message, MovedFromDoesNotCrash) {
  kind::Message m;
  m.id = 777;
  m.content = "some content";
  m.attachments.push_back(
      kind::Attachment{.id = 1, .filename = {}, .url = {}, .size = 0, .width = std::nullopt, .height = std::nullopt});
  kind::Message m2 = std::move(m);
  // Accessing moved-from object must not crash.
  // Values are unspecified but the object must be in a valid state.
  (void)m.id;
  (void)m.content;
  (void)m.attachments.size();
  (void)m.embeds.size();
  EXPECT_EQ(m2.id, 777u);
}

TEST(Guild, EmptyChannelList) {
  kind::Guild g;
  g.id = 100;
  g.name = "Test Guild";
  EXPECT_TRUE(g.channels.empty());
  EXPECT_EQ(g.channels.size(), 0u);
}

// ---------------------------------------------------------------------------
// Nonce generation
// ---------------------------------------------------------------------------

TEST(Nonce, GeneratesNonEmptyString) {
  auto nonce = kind::generate_nonce();
  EXPECT_FALSE(nonce.empty());
}

TEST(Nonce, GeneratesNumericString) {
  auto nonce = kind::generate_nonce();
  for (char ch : nonce) {
    EXPECT_TRUE(ch >= '0' && ch <= '9') << "Non-digit in nonce: " << ch;
  }
}

TEST(Nonce, ConsecutiveNoncesAreDifferent) {
  auto a = kind::generate_nonce();
  QThread::msleep(1);
  auto b = kind::generate_nonce();
  EXPECT_NE(a, b);
}

TEST(Nonce, NonceIsReasonableLength) {
  auto nonce = kind::generate_nonce();
  // Discord nonces are ~19 digits
  EXPECT_GE(nonce.size(), 15u);
  EXPECT_LE(nonce.size(), 22u);
}
