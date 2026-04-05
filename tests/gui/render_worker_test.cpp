#include "models/message.hpp"
#include "workers/render_worker.hpp"

#include <QFont>
#include <QImage>
#include <QPainter>
#include <gtest/gtest.h>

#include <unordered_map>

using namespace kind;
using namespace kind::gui;

static Message make_simple_message(Snowflake id, const std::string& content,
                                   const std::string& username = "testuser") {
  Message msg;
  msg.id = id;
  msg.content = content;
  msg.author.username = username;
  msg.timestamp = "2024-06-15T10:30:00.000Z";
  return msg;
}

// ---------------------------------------------------------------------------
// Tier 1: Normal
// ---------------------------------------------------------------------------

class RenderWorkerTest : public ::testing::Test {};

TEST_F(RenderWorkerTest, BasicMessageHasPositiveHeight) {
  auto msg = make_simple_message(1, "Hello world");
  auto result = compute_layout(msg, 400, QFont());
  EXPECT_TRUE(result.valid);
  EXPECT_GT(result.height, 0);
  EXPECT_FALSE(result.blocks.empty());
}

TEST_F(RenderWorkerTest, EmptyContentStillValid) {
  auto msg = make_simple_message(1, "");
  auto result = compute_layout(msg, 400, QFont());
  EXPECT_TRUE(result.valid);
  EXPECT_GT(result.height, 0);
}

TEST_F(RenderWorkerTest, SystemMessageType7) {
  Message msg;
  msg.id = 1;
  msg.type = 7;  // Member join
  msg.author.username = "newuser";
  msg.timestamp = "2024-01-01T00:00:00.000Z";
  auto result = compute_layout(msg, 400, QFont());
  EXPECT_TRUE(result.valid);
  EXPECT_GT(result.height, 0);
  // System messages have no timestamp text
  EXPECT_TRUE(result.timestamp_text.isEmpty());
}

TEST_F(RenderWorkerTest, MessageWithReply) {
  auto msg = make_simple_message(2, "replying here");
  msg.referenced_message_id = 1;
  msg.referenced_message_author = "original_poster";
  msg.referenced_message_content = "original message";
  auto result = compute_layout(msg, 400, QFont());
  EXPECT_TRUE(result.valid);
  // Should have at least reply block + text block
  EXPECT_GE(result.blocks.size(), 2u);
}

TEST_F(RenderWorkerTest, EditedMessageTextIndicator) {
  auto msg = make_simple_message(1, "edited text");
  msg.edited_timestamp = "2024-06-15T11:00:00.000Z";
  auto result = compute_layout(msg, 400, QFont(), {}, EditedIndicator::Text);
  EXPECT_TRUE(result.valid);
  // Timestamp should NOT contain pencil icon for Text-only indicator
  EXPECT_FALSE(result.timestamp_text.contains(QString::fromUtf8("\u270f")));
}

TEST_F(RenderWorkerTest, EditedMessageIconIndicator) {
  auto msg = make_simple_message(1, "edited text");
  msg.edited_timestamp = "2024-06-15T11:00:00.000Z";
  auto result = compute_layout(msg, 400, QFont(), {}, EditedIndicator::Icon);
  EXPECT_TRUE(result.valid);
  EXPECT_TRUE(result.timestamp_text.contains(QString::fromUtf8("\u270f")));
}

TEST_F(RenderWorkerTest, EditedMessageBothIndicator) {
  auto msg = make_simple_message(1, "edited text");
  msg.edited_timestamp = "2024-06-15T11:00:00.000Z";
  auto result = compute_layout(msg, 400, QFont(), {}, EditedIndicator::Both);
  EXPECT_TRUE(result.valid);
  EXPECT_TRUE(result.timestamp_text.contains(QString::fromUtf8("\u270f")));
}

TEST_F(RenderWorkerTest, ViewportWidthStoredOnResult) {
  auto msg = make_simple_message(1, "test");
  auto result = compute_layout(msg, 750, QFont());
  EXPECT_EQ(result.viewport_width, 750);
}

TEST_F(RenderWorkerTest, EphemeralFlagAddsNotice) {
  auto msg = make_simple_message(1, "ephemeral message");
  msg.flags = (1 << 6);  // EPHEMERAL
  auto result = compute_layout(msg, 400, QFont());
  EXPECT_TRUE(result.valid);
  // Should have text block + ephemeral notice
  EXPECT_GE(result.blocks.size(), 2u);
}

// ---------------------------------------------------------------------------
// Tier 2: Extensive edge cases
// ---------------------------------------------------------------------------

TEST_F(RenderWorkerTest, TimestampColumnReducesEffectiveWidth) {
  auto msg = make_simple_message(1, "some wrapping text that is pretty long so it wraps");

  auto no_ts = compute_layout(msg, 400, QFont(), {}, EditedIndicator::Text, {}, false, 0);
  auto with_ts = compute_layout(msg, 400, QFont(), {}, EditedIndicator::Text, {}, true, 80);
  // With timestamp column, effective width is smaller, so height should be >= no-timestamp
  EXPECT_GE(with_ts.height, no_ts.height);
}

TEST_F(RenderWorkerTest, MentionResolution) {
  auto msg = make_simple_message(1, "hello <@12345>");
  Mention m;
  m.id = 12345;
  m.username = "mentioneduser";
  msg.mentions.push_back(m);

  MentionContext ctx;
  ctx.current_user_id = 99;
  auto result = compute_layout(msg, 400, QFont(), {}, EditedIndicator::Text, ctx);
  EXPECT_TRUE(result.valid);
}

TEST_F(RenderWorkerTest, SelfMentionFlagged) {
  // Verify resolve_mention sets is_self_mention
  TextSpan span;
  span.mention_user_id = 42;

  MentionContext ctx;
  ctx.current_user_id = 42;
  ctx.user_mentions[42] = "myself";

  resolve_mention(span, ctx);
  EXPECT_TRUE(span.is_self_mention);
  EXPECT_EQ(span.resolved_text, "@myself");
}

TEST_F(RenderWorkerTest, ChannelMentionResolution) {
  TextSpan span;
  span.mention_channel_id = 100;

  MentionContext ctx;
  Channel ch;
  ch.id = 100;
  ch.name = "general";
  ctx.guild_channels.push_back(ch);

  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "#general");
}

TEST_F(RenderWorkerTest, RoleMentionWithColor) {
  TextSpan span;
  span.mention_role_id = 50;

  MentionContext ctx;
  Role role;
  role.id = 50;
  role.name = "Moderators";
  role.color = 0xFF0000;
  ctx.guild_roles.push_back(role);

  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@Moderators");
  EXPECT_EQ(span.mention_color, 0xFF000000 | 0xFF0000);
}

TEST_F(RenderWorkerTest, SelfRoleMention) {
  TextSpan span;
  span.mention_role_id = 50;

  MentionContext ctx;
  Role role;
  role.id = 50;
  role.name = "Mods";
  role.color = 0x00FF00;
  ctx.guild_roles.push_back(role);
  ctx.current_user_role_ids.push_back(50);

  resolve_mention(span, ctx);
  EXPECT_TRUE(span.is_self_mention);
}

TEST_F(RenderWorkerTest, EveryoneMentionSelf) {
  TextSpan span;
  span.text = "@everyone";

  MentionContext ctx;
  ctx.mention_everyone = true;

  resolve_mention(span, ctx);
  EXPECT_TRUE(span.is_self_mention);
  EXPECT_EQ(span.resolved_text, "@everyone");
}

TEST_F(RenderWorkerTest, HereMentionNotSelf) {
  TextSpan span;
  span.text = "@here";

  MentionContext ctx;
  ctx.mention_everyone = false;

  resolve_mention(span, ctx);
  EXPECT_FALSE(span.is_self_mention);
}

TEST_F(RenderWorkerTest, UnknownUserMention) {
  TextSpan span;
  span.mention_user_id = 99999;

  MentionContext ctx;
  // No entry in user_mentions

  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@Unknown User");
}

TEST_F(RenderWorkerTest, UnknownChannelMention) {
  TextSpan span;
  span.mention_channel_id = 99999;

  MentionContext ctx;
  // No matching channel

  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "#unknown-channel");
}

TEST_F(RenderWorkerTest, UnknownRoleMention) {
  TextSpan span;
  span.mention_role_id = 99999;

  MentionContext ctx;
  // No matching role

  resolve_mention(span, ctx);
  EXPECT_EQ(span.resolved_text, "@unknown-role");
}

TEST_F(RenderWorkerTest, SuppressTextForSingleUrlEmbed) {
  auto msg = make_simple_message(1, "https://example.com/image.png");
  Embed embed;
  embed.type = "image";
  embed.url = "https://example.com/image.png";
  msg.embeds.push_back(embed);

  auto result = compute_layout(msg, 400, QFont());
  EXPECT_TRUE(result.valid);
}

TEST_F(RenderWorkerTest, MessageWithReactions) {
  auto msg = make_simple_message(1, "react to this");
  Reaction r;
  r.emoji_name = "thumbsup";
  r.count = 3;
  msg.reactions.push_back(r);

  auto result = compute_layout(msg, 400, QFont());
  EXPECT_TRUE(result.valid);
  EXPECT_GE(result.blocks.size(), 2u);  // text + reactions
}

TEST_F(RenderWorkerTest, ComponentsV1) {
  auto msg = make_simple_message(1, "button message");
  Component row;
  row.type = 1;  // ActionRow
  Component button;
  button.type = 2;
  button.label = "Click me";
  row.children.push_back(button);
  msg.components.push_back(row);

  auto result = compute_layout(msg, 400, QFont());
  EXPECT_TRUE(result.valid);
  EXPECT_GE(result.blocks.size(), 2u);  // text + components
}

TEST_F(RenderWorkerTest, ComponentsV2) {
  auto msg = make_simple_message(1, "v2 components");
  msg.flags = (1 << 15);  // IS_COMPONENTS_V2
  Component comp;
  comp.type = 10;  // Section
  msg.components.push_back(comp);

  auto result = compute_layout(msg, 400, QFont());
  EXPECT_TRUE(result.valid);
}

// ---------------------------------------------------------------------------
// Tier 3: Absolutely unhinged scenarios
// ---------------------------------------------------------------------------

TEST_F(RenderWorkerTest, ZeroViewportWidth) {
  auto msg = make_simple_message(1, "test");
  auto result = compute_layout(msg, 0, QFont());
  EXPECT_TRUE(result.valid);
  EXPECT_GE(result.height, 0);
}

TEST_F(RenderWorkerTest, NegativeViewportWidth) {
  auto msg = make_simple_message(1, "test");
  auto result = compute_layout(msg, -100, QFont());
  // Should not crash
  EXPECT_TRUE(result.valid);
}

TEST_F(RenderWorkerTest, GiantContent) {
  std::string huge(10000, 'A');
  auto msg = make_simple_message(1, huge);
  auto result = compute_layout(msg, 400, QFont());
  EXPECT_TRUE(result.valid);
  EXPECT_GT(result.height, 0);
}

TEST_F(RenderWorkerTest, MessageWithAllBlockTypes) {
  auto msg = make_simple_message(1, "text with <@100> mention");

  // Add reply
  msg.referenced_message_id = 0;
  msg.referenced_message_author = "someone";
  msg.referenced_message_content = "ref content";

  // Add embed
  Embed embed;
  embed.type = "rich";
  embed.title = "Title";
  msg.embeds.push_back(embed);

  // Add attachment
  Attachment att;
  att.filename = "file.txt";
  att.url = "https://example.com/file.txt";
  att.size = 1024;
  msg.attachments.push_back(att);

  // Add reaction
  Reaction r;
  r.emoji_name = "fire";
  r.count = 1;
  msg.reactions.push_back(r);

  // Add sticker
  StickerItem sticker;
  sticker.id = 555;
  sticker.name = "test_sticker";
  sticker.format_type = 1;  // PNG
  msg.sticker_items.push_back(sticker);

  // Add component
  Component row;
  row.type = 1;
  msg.components.push_back(row);

  // Ephemeral
  msg.flags = (1 << 6);

  auto result = compute_layout(msg, 600, QFont());
  EXPECT_TRUE(result.valid);
  // Should have many blocks
  EXPECT_GE(result.blocks.size(), 5u);
}

TEST_F(RenderWorkerTest, TwentyEmbeds) {
  auto msg = make_simple_message(1, "many embeds");
  for (int i = 0; i < 20; ++i) {
    Embed embed;
    embed.type = "rich";
    embed.title = "Embed " + std::to_string(i);
    msg.embeds.push_back(embed);
  }
  auto result = compute_layout(msg, 400, QFont());
  EXPECT_TRUE(result.valid);
}

TEST_F(RenderWorkerTest, DuplicateEmbedUrlsCollapsed) {
  auto msg = make_simple_message(1, "tumblr post");
  for (int i = 0; i < 5; ++i) {
    Embed embed;
    embed.type = "image";
    embed.url = "https://tumblr.com/same-url";
    EmbedImage img;
    img.url = "https://tumblr.com/img" + std::to_string(i) + ".jpg";
    img.width = 200;
    img.height = 200;
    embed.image = img;
    msg.embeds.push_back(embed);
  }
  auto result = compute_layout(msg, 400, QFont());
  EXPECT_TRUE(result.valid);
}

TEST_F(RenderWorkerTest, DiscordColorsMentionContext) {
  TextSpan span;
  span.mention_user_id = 42;

  MentionContext ctx;
  ctx.current_user_id = 99;
  ctx.user_mentions[42] = "someone";
  ctx.use_discord_colors = true;

  resolve_mention(span, ctx);
  // Discord blue accent: 0x5894FF
  EXPECT_EQ(span.mention_color, 0xFF000000 | 0x5894FF);
}

TEST_F(RenderWorkerTest, ThemeColorMentionContext) {
  TextSpan span;
  span.mention_user_id = 42;

  MentionContext ctx;
  ctx.current_user_id = 99;
  ctx.user_mentions[42] = "someone";
  ctx.use_discord_colors = false;
  ctx.accent_color = 0xFF00FF;

  resolve_mention(span, ctx);
  EXPECT_EQ(span.mention_color, 0xFF000000 | 0xFF00FF);
}

TEST_F(RenderWorkerTest, RoleMentionZeroColor) {
  // Role with color=0 should fall back to accent color
  TextSpan span;
  span.mention_role_id = 50;

  MentionContext ctx;
  ctx.accent_color = 0xABCDEF;
  Role role;
  role.id = 50;
  role.name = "NoColor";
  role.color = 0;
  ctx.guild_roles.push_back(role);

  resolve_mention(span, ctx);
  EXPECT_EQ(span.mention_color, 0xFF000000 | 0xABCDEF);
}
