#include "workers/render_worker.hpp"
#include "text/markdown_parser.hpp"

#include <gtest/gtest.h>

using kind::TextSpan;
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

// Helper to create a MentionContext with a single user mention
static MentionContext make_ctx_with_mention(kind::Snowflake user_id,
                                            const std::string& username) {
  MentionContext ctx;
  ctx.user_mentions[user_id] = username;
  return ctx;
}

// ---------------------------------------------------------------------------
// Tier 1: Normal tests
// ---------------------------------------------------------------------------

TEST(MentionResolutionTest, UserMentionResolvesToUsername) {
  auto span = make_user_mention(42);
  auto ctx = make_ctx_with_mention(42, "TestUser");
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@TestUser");
  EXPECT_NE(span.mention_color, 0u);
  EXPECT_NE(span.mention_bg, 0u);
}

TEST(MentionResolutionTest, SelfMentionDetected) {
  auto span = make_user_mention(42);
  auto ctx = make_ctx_with_mention(42, "Me");
  ctx.current_user_id = 42;
  resolve_mention(span, ctx);
  EXPECT_TRUE(span.is_self_mention);
}

TEST(MentionResolutionTest, NotSelfMention) {
  auto span = make_user_mention(42);
  auto ctx = make_ctx_with_mention(42, "Other");
  ctx.current_user_id = 99;
  resolve_mention(span, ctx);
  EXPECT_FALSE(span.is_self_mention);
}

TEST(MentionResolutionTest, ChannelMentionResolvesToName) {
  TextSpan span;
  span.text = "<#200>";
  span.mention_channel_id = 200;
  MentionContext ctx;
  Channel ch;
  ch.id = 200;
  ch.name = "general";
  ctx.guild_channels.push_back(ch);
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "#general");
}

TEST(MentionResolutionTest, RoleMentionResolvesToNameWithColor) {
  TextSpan span;
  span.text = "<@&300>";
  span.mention_role_id = 300;
  MentionContext ctx;
  Role role;
  role.id = 300;
  role.name = "Moderator";
  role.color = 0xFF0000;  // red
  ctx.guild_roles.push_back(role);
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@Moderator");
  EXPECT_EQ(span.mention_color & 0x00FFFFFF, 0xFF0000u);
}

TEST(MentionResolutionTest, RoleMentionSelfDetected) {
  TextSpan span;
  span.text = "<@&300>";
  span.mention_role_id = 300;
  MentionContext ctx;
  Role role;
  role.id = 300;
  role.name = "Mod";
  role.color = 0x00FF00;
  ctx.guild_roles.push_back(role);
  ctx.current_user_role_ids.push_back(300);
  resolve_mention(span, ctx);
  EXPECT_TRUE(span.is_self_mention);
}

TEST(MentionResolutionTest, UnresolvedUserFallback) {
  auto span = make_user_mention(999);
  MentionContext ctx;  // no user_mentions entries
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@Unknown User");
}

TEST(MentionResolutionTest, UnresolvedChannelFallback) {
  TextSpan span;
  span.text = "<#999>";
  span.mention_channel_id = 999;
  MentionContext ctx;  // no channels
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "#unknown-channel");
}

TEST(MentionResolutionTest, UnresolvedRoleFallback) {
  TextSpan span;
  span.text = "<@&999>";
  span.mention_role_id = 999;
  MentionContext ctx;  // no roles
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@unknown-role");
}

TEST(MentionResolutionTest, EveryoneMentionStyled) {
  TextSpan span;
  span.text = "@everyone";
  MentionContext ctx;
  ctx.mention_everyone = true;
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@everyone");
  EXPECT_NE(span.mention_color, 0u);
  EXPECT_TRUE(span.is_self_mention);
}

TEST(MentionResolutionTest, HereMentionStyled) {
  TextSpan span;
  span.text = "@here";
  MentionContext ctx;
  ctx.mention_everyone = true;
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@here");
  EXPECT_TRUE(span.is_self_mention);
}

// ---------------------------------------------------------------------------
// Tier 1.5: New tests for self-contained MentionContext
// ---------------------------------------------------------------------------

TEST(MentionResolutionTest, UserMentionFromContext) {
  auto span = make_user_mention(77);
  MentionContext ctx;
  ctx.user_mentions[77] = "CtxUser";
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@CtxUser");
  EXPECT_NE(span.mention_color, 0u);
}

TEST(MentionResolutionTest, MentionEveryoneFromContext) {
  TextSpan span;
  span.text = "@everyone";
  MentionContext ctx;
  ctx.mention_everyone = true;
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@everyone");
  EXPECT_TRUE(span.is_self_mention);
}

TEST(MentionResolutionTest, UnknownUserFromContext) {
  auto span = make_user_mention(12345);
  MentionContext ctx;
  // user_mentions is empty, so this ID should not resolve
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@Unknown User");
}

// ---------------------------------------------------------------------------
// Tier 2: Extensive edge case tests
// ---------------------------------------------------------------------------

TEST(MentionResolutionTest, DiscordColorPreference) {
  auto span = make_user_mention(42);
  auto ctx = make_ctx_with_mention(42, "User");
  ctx.use_discord_colors = true;
  resolve_mention(span, ctx);
  EXPECT_EQ(span.mention_color & 0x00FFFFFF, 0x5894FFu);
}

TEST(MentionResolutionTest, ThemeAccentColorUsed) {
  auto span = make_user_mention(42);
  auto ctx = make_ctx_with_mention(42, "User");
  ctx.accent_color = 0xAABBCC;
  ctx.use_discord_colors = false;
  resolve_mention(span, ctx);
  EXPECT_EQ(span.mention_color & 0x00FFFFFF, 0xAABBCCu);
}

TEST(MentionResolutionTest, RoleWithZeroColorUsesAccent) {
  TextSpan span;
  span.text = "<@&300>";
  span.mention_role_id = 300;
  MentionContext ctx;
  ctx.accent_color = 0x112233;
  Role role;
  role.id = 300;
  role.name = "NoColor";
  role.color = 0;  // no color set
  ctx.guild_roles.push_back(role);
  resolve_mention(span, ctx);
  EXPECT_EQ(span.mention_color & 0x00FFFFFF, 0x112233u);
}

TEST(MentionResolutionTest, PlainTextNotAffected) {
  TextSpan span;
  span.text = "hello world";
  MentionContext ctx;
  resolve_mention(span, ctx);
  EXPECT_TRUE(span.resolved_text.empty());
  EXPECT_EQ(span.mention_color, 0u);
}

TEST(MentionResolutionTest, SelfMentionStrongerBackground) {
  auto span = make_user_mention(42);
  auto ctx = make_ctx_with_mention(42, "Me");
  ctx.current_user_id = 42;
  resolve_mention(span, ctx);
  uint8_t alpha = (span.mention_bg >> 24) & 0xFF;
  EXPECT_GE(alpha, 0x3C);  // self mention alpha is 0x3C
}

TEST(MentionResolutionTest, NormalMentionWeakerBackground) {
  auto span = make_user_mention(42);
  auto ctx = make_ctx_with_mention(42, "Other");
  ctx.current_user_id = 99;
  resolve_mention(span, ctx);
  uint8_t alpha = (span.mention_bg >> 24) & 0xFF;
  EXPECT_EQ(alpha, 0x1E);  // normal mention alpha is 0x1E
}

TEST(MentionResolutionTest, ChannelMentionColorMatchesAccent) {
  TextSpan span;
  span.text = "<#200>";
  span.mention_channel_id = 200;
  MentionContext ctx;
  ctx.accent_color = 0xDDEEFF;
  Channel ch;
  ch.id = 200;
  ch.name = "dev";
  ctx.guild_channels.push_back(ch);
  resolve_mention(span, ctx);
  EXPECT_EQ(span.mention_color & 0x00FFFFFF, 0xDDEEFFu);
}

TEST(MentionResolutionTest, EveryoneWithoutMentionEveryoneFlag) {
  TextSpan span;
  span.text = "@everyone";
  MentionContext ctx;
  ctx.mention_everyone = false;
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@everyone");
  EXPECT_FALSE(span.is_self_mention);
  // Still styled as a mention, just not self
  EXPECT_NE(span.mention_color, 0u);
}

TEST(MentionResolutionTest, RoleSelfMentionStrongerBackground) {
  TextSpan span;
  span.text = "<@&300>";
  span.mention_role_id = 300;
  MentionContext ctx;
  Role role;
  role.id = 300;
  role.name = "Admin";
  role.color = 0xABCDEF;
  ctx.guild_roles.push_back(role);
  ctx.current_user_role_ids.push_back(300);
  resolve_mention(span, ctx);
  uint8_t alpha = (span.mention_bg >> 24) & 0xFF;
  EXPECT_EQ(alpha, 0x3C);
  EXPECT_EQ(span.mention_bg & 0x00FFFFFF, 0xABCDEFu);
}

TEST(MentionResolutionTest, RoleNonSelfMentionWeakerBackground) {
  TextSpan span;
  span.text = "<@&300>";
  span.mention_role_id = 300;
  MentionContext ctx;
  Role role;
  role.id = 300;
  role.name = "Admin";
  role.color = 0xABCDEF;
  ctx.guild_roles.push_back(role);
  // current_user_role_ids is empty, so not self
  resolve_mention(span, ctx);
  uint8_t alpha = (span.mention_bg >> 24) & 0xFF;
  EXPECT_EQ(alpha, 0x1E);
}

TEST(MentionResolutionTest, MultipleRolesOnlyMatchingOneIsSelf) {
  TextSpan span;
  span.text = "<@&300>";
  span.mention_role_id = 300;
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
  resolve_mention(span, ctx);
  EXPECT_TRUE(span.is_self_mention);
}

TEST(MentionResolutionTest, FullAlphaOnForegroundColor) {
  auto span = make_user_mention(42);
  auto ctx = make_ctx_with_mention(42, "User");
  ctx.accent_color = 0x00FF00;
  resolve_mention(span, ctx);
  // Foreground always has 0xFF alpha
  EXPECT_EQ(span.mention_color >> 24, 0xFFu);
}

// ---------------------------------------------------------------------------
// Tier 3: Absolutely unhinged scenarios
// ---------------------------------------------------------------------------

TEST(MentionResolutionTest, HundredMentionsInContext) {
  MentionContext ctx;
  for (int i = 0; i < 100; ++i) {
    ctx.user_mentions[static_cast<kind::Snowflake>(1000 + i)] =
        "user" + std::to_string(i);
  }
  for (int i = 0; i < 100; ++i) {
    auto span = make_user_mention(1000 + i);
    resolve_mention(span, ctx);
    EXPECT_EQ(span.resolved_text, "@user" + std::to_string(i));
  }
}

TEST(MentionResolutionTest, ZeroSnowflakeId) {
  auto span = make_user_mention(0);
  MentionContext ctx;
  ctx.user_mentions[0] = "ZeroUser";
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@ZeroUser");
}

TEST(MentionResolutionTest, MaxSnowflakeId) {
  kind::Snowflake max_id = std::numeric_limits<kind::Snowflake>::max();
  auto span = make_user_mention(max_id);
  MentionContext ctx;
  ctx.user_mentions[max_id] = "MaxUser";
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@MaxUser");
}

TEST(MentionResolutionTest, EmptyUsername) {
  auto span = make_user_mention(42);
  MentionContext ctx;
  ctx.user_mentions[42] = "";
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@");
}

TEST(MentionResolutionTest, UsernameWithSpecialCharacters) {
  auto span = make_user_mention(42);
  MentionContext ctx;
  ctx.user_mentions[42] = "user <script>alert('xss')</script>";
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@user <script>alert('xss')</script>");
}

TEST(MentionResolutionTest, DuplicateMentionEntriesLastWins) {
  // With unordered_map, inserting the same key twice overwrites, so last wins
  auto span = make_user_mention(42);
  MentionContext ctx;
  ctx.user_mentions[42] = "First";
  ctx.user_mentions[42] = "Second";
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@Second");
}

TEST(MentionResolutionTest, ResolveCalledTwiceOverwrites) {
  auto span = make_user_mention(42);
  auto ctx1 = make_ctx_with_mention(42, "First");
  auto ctx2 = make_ctx_with_mention(42, "Second");
  resolve_mention(span, ctx1);
  EXPECT_EQ(span.resolved_text, "@First");
  resolve_mention(span, ctx2);
  EXPECT_EQ(span.resolved_text, "@Second");
}

TEST(MentionResolutionTest, SelfMentionFlagResetOnReResolve) {
  auto span = make_user_mention(42);
  auto ctx = make_ctx_with_mention(42, "User");
  ctx.current_user_id = 42;
  resolve_mention(span, ctx);
  EXPECT_TRUE(span.is_self_mention);
  // Re-resolve with a different context where it is not self
  auto ctx2 = make_ctx_with_mention(42, "User");
  ctx2.current_user_id = 99;
  resolve_mention(span, ctx2);
  EXPECT_FALSE(span.is_self_mention);
}

TEST(MentionResolutionTest, AllThreeTypesInSequence) {
  MentionContext ctx;
  ctx.accent_color = 0xAAAAAA;
  ctx.user_mentions[42] = "Alice";
  Role role;
  role.id = 300;
  role.name = "Dev";
  role.color = 0xBBBBBB;
  ctx.guild_roles.push_back(role);
  Channel ch;
  ch.id = 200;
  ch.name = "chat";
  ctx.guild_channels.push_back(ch);

  auto user_span = make_user_mention(42);
  resolve_mention(user_span, ctx);
  EXPECT_EQ(user_span.resolved_text, "@Alice");

  TextSpan chan_span;
  chan_span.text = "<#200>";
  chan_span.mention_channel_id = 200;
  resolve_mention(chan_span, ctx);
  EXPECT_EQ(chan_span.resolved_text, "#chat");

  TextSpan role_span;
  role_span.text = "<@&300>";
  role_span.mention_role_id = 300;
  resolve_mention(role_span, ctx);
  EXPECT_EQ(role_span.resolved_text, "@Dev");
  EXPECT_EQ(role_span.mention_color & 0x00FFFFFF, 0xBBBBBBu);
}

TEST(MentionResolutionTest, AtEveryoneTextNotAtEveryone) {
  // Text that looks like @everyone but is just plain text (no mention flags)
  TextSpan span;
  span.text = "I said @everyone in a sentence";
  MentionContext ctx;
  resolve_mention(span, ctx);
  // Should not be resolved since text != "@everyone" exactly
  EXPECT_TRUE(span.resolved_text.empty());
}

TEST(MentionResolutionTest, ManyRolesInContext) {
  TextSpan span;
  span.text = "<@&450>";
  span.mention_role_id = 450;
  MentionContext ctx;
  // Add 200 roles with IDs 300..499, target 450 is at index 150
  for (int i = 0; i < 200; ++i) {
    Role r;
    r.id = static_cast<kind::Snowflake>(300 + i);
    r.name = "Role" + std::to_string(i);
    r.color = static_cast<uint32_t>(0x010000 * (i + 1));
    ctx.guild_roles.push_back(r);
  }
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@Role150");
  EXPECT_EQ(span.mention_color & 0x00FFFFFF, static_cast<uint32_t>(0x010000 * 151));
}

TEST(MentionResolutionTest, ManyChannelsInContext) {
  TextSpan span;
  span.text = "<#550>";
  span.mention_channel_id = 550;
  MentionContext ctx;
  for (int i = 0; i < 200; ++i) {
    Channel c;
    c.id = static_cast<kind::Snowflake>(400 + i);
    c.name = "channel-" + std::to_string(i);
    ctx.guild_channels.push_back(c);
  }
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "#channel-150");
}

TEST(MentionResolutionTest, UnicodeChannelName) {
  TextSpan span;
  span.text = "<#200>";
  span.mention_channel_id = 200;
  MentionContext ctx;
  Channel ch;
  ch.id = 200;
  ch.name = "\xe4\xb8\xad\xe6\x96\x87\xe9\xa2\x91\xe9\x81\x93";  // Chinese characters
  ctx.guild_channels.push_back(ch);
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "#\xe4\xb8\xad\xe6\x96\x87\xe9\xa2\x91\xe9\x81\x93");
}

TEST(MentionResolutionTest, UnicodeRoleName) {
  TextSpan span;
  span.text = "<@&300>";
  span.mention_role_id = 300;
  MentionContext ctx;
  Role role;
  role.id = 300;
  role.name = "\xf0\x9f\x94\xa5 Fire Squad";  // emoji in role name
  role.color = 0xFF4500;
  ctx.guild_roles.push_back(role);
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@\xf0\x9f\x94\xa5 Fire Squad");
}

TEST(MentionResolutionTest, ThousandUsersInContext) {
  MentionContext ctx;
  for (int i = 0; i < 5000; ++i) {
    ctx.user_mentions[static_cast<kind::Snowflake>(i)] =
        "user" + std::to_string(i);
  }
  // Resolve the very last entry to verify lookup works across a large map
  auto span = make_user_mention(4999);
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@user4999");
}

TEST(MentionResolutionTest, EmptyUserMentionsMap) {
  auto span = make_user_mention(42);
  MentionContext ctx;
  // user_mentions is empty, should fall back
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@Unknown User");
}

TEST(MentionResolutionTest, MentionEveryoneFalseInContext) {
  TextSpan span;
  span.text = "@everyone";
  MentionContext ctx;
  ctx.mention_everyone = false;
  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@everyone");
  EXPECT_FALSE(span.is_self_mention);
}
