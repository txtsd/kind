#include "workers/render_worker.hpp"
#include "text/markdown_parser.hpp"
#include "models/message.hpp"

#include <gtest/gtest.h>

using kind::TextSpan;
using kind::Message;
using kind::Mention;
using kind::Role;
using kind::Channel;
using kind::gui::MentionContext;
using kind::gui::resolve_mention;

// Helper to create a span with a user mention
static TextSpan make_user_mention(kind::Snowflake id) {
  TextSpan span;
  span.text = "<@" + std::to_string(id) + ">";
  span.mention_user_id = id;
  return span;
}

// Helper to create a message with a single user mention
static Message make_message_with_mention(kind::Snowflake user_id,
                                         const std::string& username) {
  Message msg;
  msg.id = 1;
  msg.channel_id = 100;
  msg.timestamp = "2026-01-01T00:00:00.000Z";
  Mention m;
  m.id = user_id;
  m.username = username;
  msg.mentions.push_back(m);
  return msg;
}

// ---------------------------------------------------------------------------
// Tier 1: Normal tests
// ---------------------------------------------------------------------------

TEST(MentionResolutionTest, UserMentionResolvesToUsername) {
  auto span = make_user_mention(42);
  auto msg = make_message_with_mention(42, "TestUser");
  MentionContext ctx;
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "@TestUser");
  EXPECT_NE(span.mention_color, 0u);
  EXPECT_NE(span.mention_bg, 0u);
}

TEST(MentionResolutionTest, SelfMentionDetected) {
  auto span = make_user_mention(42);
  auto msg = make_message_with_mention(42, "Me");
  MentionContext ctx;
  ctx.current_user_id = 42;
  resolve_mention(span, msg, ctx);
  EXPECT_TRUE(span.is_self_mention);
}

TEST(MentionResolutionTest, NotSelfMention) {
  auto span = make_user_mention(42);
  auto msg = make_message_with_mention(42, "Other");
  MentionContext ctx;
  ctx.current_user_id = 99;
  resolve_mention(span, msg, ctx);
  EXPECT_FALSE(span.is_self_mention);
}

TEST(MentionResolutionTest, ChannelMentionResolvesToName) {
  TextSpan span;
  span.text = "<#200>";
  span.mention_channel_id = 200;
  Message msg;
  MentionContext ctx;
  Channel ch;
  ch.id = 200;
  ch.name = "general";
  ctx.guild_channels.push_back(ch);
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "#general");
}

TEST(MentionResolutionTest, RoleMentionResolvesToNameWithColor) {
  TextSpan span;
  span.text = "<@&300>";
  span.mention_role_id = 300;
  Message msg;
  MentionContext ctx;
  Role role;
  role.id = 300;
  role.name = "Moderator";
  role.color = 0xFF0000;  // red
  ctx.guild_roles.push_back(role);
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "@Moderator");
  EXPECT_EQ(span.mention_color & 0x00FFFFFF, 0xFF0000u);
}

TEST(MentionResolutionTest, RoleMentionSelfDetected) {
  TextSpan span;
  span.text = "<@&300>";
  span.mention_role_id = 300;
  Message msg;
  MentionContext ctx;
  Role role;
  role.id = 300;
  role.name = "Mod";
  role.color = 0x00FF00;
  ctx.guild_roles.push_back(role);
  ctx.current_user_role_ids.push_back(300);
  resolve_mention(span, msg, ctx);
  EXPECT_TRUE(span.is_self_mention);
}

TEST(MentionResolutionTest, UnresolvedUserFallback) {
  auto span = make_user_mention(999);
  Message msg;  // no mentions array entries
  MentionContext ctx;
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "@Unknown User");
}

TEST(MentionResolutionTest, UnresolvedChannelFallback) {
  TextSpan span;
  span.text = "<#999>";
  span.mention_channel_id = 999;
  Message msg;
  MentionContext ctx;  // no channels
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "#unknown-channel");
}

TEST(MentionResolutionTest, UnresolvedRoleFallback) {
  TextSpan span;
  span.text = "<@&999>";
  span.mention_role_id = 999;
  Message msg;
  MentionContext ctx;  // no roles
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "@unknown-role");
}

TEST(MentionResolutionTest, EveryoneMentionStyled) {
  TextSpan span;
  span.text = "@everyone";
  Message msg;
  msg.mention_everyone = true;
  MentionContext ctx;
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "@everyone");
  EXPECT_NE(span.mention_color, 0u);
  EXPECT_TRUE(span.is_self_mention);
}

TEST(MentionResolutionTest, HereMentionStyled) {
  TextSpan span;
  span.text = "@here";
  Message msg;
  msg.mention_everyone = true;
  MentionContext ctx;
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "@here");
  EXPECT_TRUE(span.is_self_mention);
}

// ---------------------------------------------------------------------------
// Tier 2: Extensive edge case tests
// ---------------------------------------------------------------------------

TEST(MentionResolutionTest, DiscordColorPreference) {
  auto span = make_user_mention(42);
  auto msg = make_message_with_mention(42, "User");
  MentionContext ctx;
  ctx.use_discord_colors = true;
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.mention_color & 0x00FFFFFF, 0x5894FFu);
}

TEST(MentionResolutionTest, ThemeAccentColorUsed) {
  auto span = make_user_mention(42);
  auto msg = make_message_with_mention(42, "User");
  MentionContext ctx;
  ctx.accent_color = 0xAABBCC;
  ctx.use_discord_colors = false;
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.mention_color & 0x00FFFFFF, 0xAABBCCu);
}

TEST(MentionResolutionTest, RoleWithZeroColorUsesAccent) {
  TextSpan span;
  span.text = "<@&300>";
  span.mention_role_id = 300;
  Message msg;
  MentionContext ctx;
  ctx.accent_color = 0x112233;
  Role role;
  role.id = 300;
  role.name = "NoColor";
  role.color = 0;  // no color set
  ctx.guild_roles.push_back(role);
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.mention_color & 0x00FFFFFF, 0x112233u);
}

TEST(MentionResolutionTest, PlainTextNotAffected) {
  TextSpan span;
  span.text = "hello world";
  Message msg;
  MentionContext ctx;
  resolve_mention(span, msg, ctx);
  EXPECT_TRUE(span.resolved_text.empty());
  EXPECT_EQ(span.mention_color, 0u);
}

TEST(MentionResolutionTest, SelfMentionStrongerBackground) {
  auto span = make_user_mention(42);
  auto msg = make_message_with_mention(42, "Me");
  MentionContext ctx;
  ctx.current_user_id = 42;
  resolve_mention(span, msg, ctx);
  uint8_t alpha = (span.mention_bg >> 24) & 0xFF;
  EXPECT_GE(alpha, 0x3C);  // self mention alpha is 0x3C
}

TEST(MentionResolutionTest, NormalMentionWeakerBackground) {
  auto span = make_user_mention(42);
  auto msg = make_message_with_mention(42, "Other");
  MentionContext ctx;
  ctx.current_user_id = 99;
  resolve_mention(span, msg, ctx);
  uint8_t alpha = (span.mention_bg >> 24) & 0xFF;
  EXPECT_EQ(alpha, 0x1E);  // normal mention alpha is 0x1E
}

TEST(MentionResolutionTest, ChannelMentionColorMatchesAccent) {
  TextSpan span;
  span.text = "<#200>";
  span.mention_channel_id = 200;
  Message msg;
  MentionContext ctx;
  ctx.accent_color = 0xDDEEFF;
  Channel ch;
  ch.id = 200;
  ch.name = "dev";
  ctx.guild_channels.push_back(ch);
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.mention_color & 0x00FFFFFF, 0xDDEEFFu);
}

TEST(MentionResolutionTest, EveryoneWithoutMentionEveryoneFlag) {
  TextSpan span;
  span.text = "@everyone";
  Message msg;
  msg.mention_everyone = false;
  MentionContext ctx;
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "@everyone");
  EXPECT_FALSE(span.is_self_mention);
  // Still styled as a mention, just not self
  EXPECT_NE(span.mention_color, 0u);
}

TEST(MentionResolutionTest, RoleSelfMentionStrongerBackground) {
  TextSpan span;
  span.text = "<@&300>";
  span.mention_role_id = 300;
  Message msg;
  MentionContext ctx;
  Role role;
  role.id = 300;
  role.name = "Admin";
  role.color = 0xABCDEF;
  ctx.guild_roles.push_back(role);
  ctx.current_user_role_ids.push_back(300);
  resolve_mention(span, msg, ctx);
  uint8_t alpha = (span.mention_bg >> 24) & 0xFF;
  EXPECT_EQ(alpha, 0x3C);
  EXPECT_EQ(span.mention_bg & 0x00FFFFFF, 0xABCDEFu);
}

TEST(MentionResolutionTest, RoleNonSelfMentionWeakerBackground) {
  TextSpan span;
  span.text = "<@&300>";
  span.mention_role_id = 300;
  Message msg;
  MentionContext ctx;
  Role role;
  role.id = 300;
  role.name = "Admin";
  role.color = 0xABCDEF;
  ctx.guild_roles.push_back(role);
  // current_user_role_ids is empty, so not self
  resolve_mention(span, msg, ctx);
  uint8_t alpha = (span.mention_bg >> 24) & 0xFF;
  EXPECT_EQ(alpha, 0x1E);
}

TEST(MentionResolutionTest, MultipleRolesOnlyMatchingOneIsSelf) {
  TextSpan span;
  span.text = "<@&300>";
  span.mention_role_id = 300;
  Message msg;
  MentionContext ctx;
  Role role;
  role.id = 300;
  role.name = "Target";
  role.color = 0x123456;
  ctx.guild_roles.push_back(role);
  // User has several roles, including the target one
  ctx.current_user_role_ids.push_back(100);
  ctx.current_user_role_ids.push_back(200);
  ctx.current_user_role_ids.push_back(300);
  ctx.current_user_role_ids.push_back(400);
  resolve_mention(span, msg, ctx);
  EXPECT_TRUE(span.is_self_mention);
}

TEST(MentionResolutionTest, FullAlphaOnForegroundColor) {
  auto span = make_user_mention(42);
  auto msg = make_message_with_mention(42, "User");
  MentionContext ctx;
  ctx.accent_color = 0x00FF00;
  resolve_mention(span, msg, ctx);
  // Foreground always has 0xFF alpha
  EXPECT_EQ(span.mention_color >> 24, 0xFFu);
}

// ---------------------------------------------------------------------------
// Tier 3: Absolutely unhinged scenarios
// ---------------------------------------------------------------------------

TEST(MentionResolutionTest, HundredMentionsInMessage) {
  Message msg;
  msg.id = 1;
  for (int i = 0; i < 100; ++i) {
    Mention m;
    m.id = static_cast<kind::Snowflake>(1000 + i);
    m.username = "user" + std::to_string(i);
    msg.mentions.push_back(m);
  }
  MentionContext ctx;
  for (int i = 0; i < 100; ++i) {
    auto span = make_user_mention(1000 + i);
    resolve_mention(span, msg, ctx);
    EXPECT_EQ(span.resolved_text, "@user" + std::to_string(i));
  }
}

TEST(MentionResolutionTest, ZeroSnowflakeId) {
  auto span = make_user_mention(0);
  Message msg;
  Mention m;
  m.id = 0;
  m.username = "ZeroUser";
  msg.mentions.push_back(m);
  MentionContext ctx;
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "@ZeroUser");
}

TEST(MentionResolutionTest, MaxSnowflakeId) {
  kind::Snowflake max_id = std::numeric_limits<kind::Snowflake>::max();
  auto span = make_user_mention(max_id);
  Message msg;
  Mention m;
  m.id = max_id;
  m.username = "MaxUser";
  msg.mentions.push_back(m);
  MentionContext ctx;
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "@MaxUser");
}

TEST(MentionResolutionTest, EmptyUsername) {
  auto span = make_user_mention(42);
  Message msg;
  Mention m;
  m.id = 42;
  m.username = "";
  msg.mentions.push_back(m);
  MentionContext ctx;
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "@");
}

TEST(MentionResolutionTest, UsernameWithSpecialCharacters) {
  auto span = make_user_mention(42);
  Message msg;
  Mention m;
  m.id = 42;
  m.username = "user <script>alert('xss')</script>";
  msg.mentions.push_back(m);
  MentionContext ctx;
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "@user <script>alert('xss')</script>");
}

TEST(MentionResolutionTest, DuplicateMentionEntriesFirstWins) {
  auto span = make_user_mention(42);
  Message msg;
  Mention m1;
  m1.id = 42;
  m1.username = "First";
  msg.mentions.push_back(m1);
  Mention m2;
  m2.id = 42;
  m2.username = "Second";
  msg.mentions.push_back(m2);
  MentionContext ctx;
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "@First");
}

TEST(MentionResolutionTest, ResolveCalledTwiceOverwrites) {
  auto span = make_user_mention(42);
  auto msg1 = make_message_with_mention(42, "First");
  auto msg2 = make_message_with_mention(42, "Second");
  MentionContext ctx;
  resolve_mention(span, msg1, ctx);
  EXPECT_EQ(span.resolved_text, "@First");
  resolve_mention(span, msg2, ctx);
  EXPECT_EQ(span.resolved_text, "@Second");
}

TEST(MentionResolutionTest, SelfMentionFlagResetOnReResolve) {
  auto span = make_user_mention(42);
  auto msg = make_message_with_mention(42, "User");
  MentionContext ctx;
  ctx.current_user_id = 42;
  resolve_mention(span, msg, ctx);
  EXPECT_TRUE(span.is_self_mention);
  // Re-resolve with a different context where it is not self
  MentionContext ctx2;
  ctx2.current_user_id = 99;
  // Note: is_self_mention is not reset by the function, it only sets it to true.
  // This documents current behavior, not necessarily desired behavior.
  resolve_mention(span, msg, ctx2);
  // The function only sets is_self_mention = true, never resets it.
  // So after calling resolve with non-self context, it stays true from previous call.
  EXPECT_TRUE(span.is_self_mention);
}

TEST(MentionResolutionTest, AllThreeTypesInSequence) {
  MentionContext ctx;
  ctx.accent_color = 0xAAAAAA;
  Role role;
  role.id = 300;
  role.name = "Dev";
  role.color = 0xBBBBBB;
  ctx.guild_roles.push_back(role);
  Channel ch;
  ch.id = 200;
  ch.name = "chat";
  ctx.guild_channels.push_back(ch);
  Message msg;
  Mention m;
  m.id = 42;
  m.username = "Alice";
  msg.mentions.push_back(m);

  auto user_span = make_user_mention(42);
  resolve_mention(user_span, msg, ctx);
  EXPECT_EQ(user_span.resolved_text, "@Alice");

  TextSpan chan_span;
  chan_span.text = "<#200>";
  chan_span.mention_channel_id = 200;
  resolve_mention(chan_span, msg, ctx);
  EXPECT_EQ(chan_span.resolved_text, "#chat");

  TextSpan role_span;
  role_span.text = "<@&300>";
  role_span.mention_role_id = 300;
  resolve_mention(role_span, msg, ctx);
  EXPECT_EQ(role_span.resolved_text, "@Dev");
  EXPECT_EQ(role_span.mention_color & 0x00FFFFFF, 0xBBBBBBu);
}

TEST(MentionResolutionTest, AtEveryoneTextNotAtEveryone) {
  // Text that looks like @everyone but is just plain text (no mention flags)
  TextSpan span;
  span.text = "I said @everyone in a sentence";
  Message msg;
  MentionContext ctx;
  resolve_mention(span, msg, ctx);
  // Should not be resolved since text != "@everyone" exactly
  EXPECT_TRUE(span.resolved_text.empty());
}

TEST(MentionResolutionTest, ManyRolesInContext) {
  TextSpan span;
  span.text = "<@&450>";
  span.mention_role_id = 450;
  Message msg;
  MentionContext ctx;
  // Add 200 roles with IDs 300..499, target 450 is at index 150
  for (int i = 0; i < 200; ++i) {
    Role r;
    r.id = static_cast<kind::Snowflake>(300 + i);
    r.name = "Role" + std::to_string(i);
    r.color = static_cast<uint32_t>(0x010000 * (i + 1));
    ctx.guild_roles.push_back(r);
  }
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "@Role150");
  EXPECT_EQ(span.mention_color & 0x00FFFFFF, static_cast<uint32_t>(0x010000 * 151));
}

TEST(MentionResolutionTest, ManyChannelsInContext) {
  TextSpan span;
  span.text = "<#550>";
  span.mention_channel_id = 550;
  Message msg;
  MentionContext ctx;
  for (int i = 0; i < 200; ++i) {
    Channel c;
    c.id = static_cast<kind::Snowflake>(400 + i);
    c.name = "channel-" + std::to_string(i);
    ctx.guild_channels.push_back(c);
  }
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "#channel-150");
}

TEST(MentionResolutionTest, UnicodeChannelName) {
  TextSpan span;
  span.text = "<#200>";
  span.mention_channel_id = 200;
  Message msg;
  MentionContext ctx;
  Channel ch;
  ch.id = 200;
  ch.name = "\xe4\xb8\xad\xe6\x96\x87\xe9\xa2\x91\xe9\x81\x93";  // Chinese characters
  ctx.guild_channels.push_back(ch);
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "#\xe4\xb8\xad\xe6\x96\x87\xe9\xa2\x91\xe9\x81\x93");
}

TEST(MentionResolutionTest, UnicodeRoleName) {
  TextSpan span;
  span.text = "<@&300>";
  span.mention_role_id = 300;
  Message msg;
  MentionContext ctx;
  Role role;
  role.id = 300;
  role.name = "\xf0\x9f\x94\xa5 Fire Squad";  // emoji in role name
  role.color = 0xFF4500;
  ctx.guild_roles.push_back(role);
  resolve_mention(span, msg, ctx);
  EXPECT_EQ(span.resolved_text, "@\xf0\x9f\x94\xa5 Fire Squad");
}
