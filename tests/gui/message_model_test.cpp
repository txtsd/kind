#include "models/message_model.hpp"

#include "models/attachment.hpp"
#include "models/embed.hpp"
#include "models/message.hpp"

#include <gtest/gtest.h>

static kind::Message make_msg(kind::Snowflake id) {
  kind::Message msg;
  msg.id = id;
  msg.channel_id = 1;
  msg.content = "test";
  msg.timestamp = "2026-01-01T00:00:00.000Z";
  msg.author.username = "user";
  return msg;
}

static kind::Message make_msg_with_attachment(kind::Snowflake id,
                                               const std::string& attachment_url) {
  auto msg = make_msg(id);
  kind::Attachment att;
  att.id = id * 1000; // deterministic but distinct from message id
  att.filename = "image.png";
  att.url = attachment_url;
  att.proxy_url = "https://media.discordapp.net/attachments/proxy/" + std::to_string(id);
  att.size = 12345;
  att.content_type = "image/png";
  att.width = 800;
  att.height = 600;
  msg.attachments.push_back(std::move(att));
  return msg;
}

class MessageModelTest : public ::testing::Test {
protected:
  kind::gui::MessageModel model_;
};

TEST_F(MessageModelTest, RowForIdFindsExisting) {
  model_.set_messages({make_msg(10), make_msg(20), make_msg(30)});
  EXPECT_EQ(model_.row_for_id(20), 1);
}

TEST_F(MessageModelTest, RowForIdReturnsNulloptForMissing) {
  model_.set_messages({make_msg(10), make_msg(30)});
  EXPECT_EQ(model_.row_for_id(20), std::nullopt);
}

TEST_F(MessageModelTest, RowForIdEmptyModel) {
  EXPECT_EQ(model_.row_for_id(1), std::nullopt);
}

TEST_F(MessageModelTest, RowForIdFirstAndLast) {
  model_.set_messages({make_msg(10), make_msg(20), make_msg(30)});
  EXPECT_EQ(model_.row_for_id(10), 0);
  EXPECT_EQ(model_.row_for_id(30), 2);
}

// =============================================================================
// Tier 1: has_content_changes basic functionality
// =============================================================================

TEST_F(MessageModelTest, HasContentChangesReturnsTrueForEmptyModel) {
  // Model is empty, incoming has one message: that is a change.
  std::vector<kind::Message> incoming = {make_msg(1)};
  EXPECT_TRUE(model_.has_content_changes(incoming));
}

TEST_F(MessageModelTest, HasContentChangesReturnsFalseForIdenticalMessages) {
  auto msgs = std::vector{make_msg(10), make_msg(20), make_msg(30)};
  model_.set_messages(msgs);

  // Same messages, sorted by id (set_messages sorts internally)
  std::vector<kind::Message> sorted = {make_msg(10), make_msg(20), make_msg(30)};
  EXPECT_FALSE(model_.has_content_changes(sorted));
}

TEST_F(MessageModelTest, HasContentChangesReturnsTrueForContentChange) {
  model_.set_messages({make_msg(10), make_msg(20)});

  auto modified = std::vector{make_msg(10), make_msg(20)};
  modified[1].content = "edited content";
  EXPECT_TRUE(model_.has_content_changes(modified));
}

TEST_F(MessageModelTest, HasContentChangesReturnsFalseWhenOnlyAttachmentUrlsDiffer) {
  // This is the critical test for the merge optimization: attachment signature
  // rotation changes the url and proxy_url fields, but attachments_content_equal
  // intentionally ignores those fields.
  auto msg1 = make_msg_with_attachment(
      10, "https://cdn.discordapp.com/attachments/1/2/img.png?ex=aaa&is=bbb&hm=ccc");
  auto msg2 = make_msg_with_attachment(
      20, "https://cdn.discordapp.com/attachments/3/4/photo.png?ex=ddd&is=eee&hm=fff");
  model_.set_messages({msg1, msg2});

  // Same messages but with rotated signatures (different url and proxy_url)
  auto msg1_rotated = make_msg_with_attachment(
      10, "https://cdn.discordapp.com/attachments/1/2/img.png?ex=xxx&is=yyy&hm=zzz");
  auto msg2_rotated = make_msg_with_attachment(
      20, "https://cdn.discordapp.com/attachments/3/4/photo.png?ex=111&is=222&hm=333");
  // proxy_url also differs
  msg1_rotated.attachments[0].proxy_url = "https://media.discordapp.net/rotated/10";
  msg2_rotated.attachments[0].proxy_url = "https://media.discordapp.net/rotated/20";

  EXPECT_FALSE(model_.has_content_changes({msg1_rotated, msg2_rotated}));
}

// =============================================================================
// Tier 2: has_content_changes edge cases
// =============================================================================

TEST_F(MessageModelTest, HasContentChangesReturnsTrueForReactionChange) {
  auto msg = make_msg(10);
  kind::Reaction reaction;
  reaction.emoji_name = "thumbsup";
  reaction.count = 1;
  reaction.me = false;
  msg.reactions.push_back(reaction);
  model_.set_messages({msg});

  // Change the reaction count
  auto modified = msg;
  modified.reactions[0].count = 2;
  EXPECT_TRUE(model_.has_content_changes({modified}));

  // Reset and change the me flag instead
  auto modified2 = msg;
  modified2.reactions[0].me = true;
  EXPECT_TRUE(model_.has_content_changes({modified2}));
}

TEST_F(MessageModelTest, HasContentChangesReturnsTrueForDifferentMessageCount) {
  model_.set_messages({make_msg(10), make_msg(20)});

  // One fewer message
  EXPECT_TRUE(model_.has_content_changes({make_msg(10)}));

  // One more message
  EXPECT_TRUE(model_.has_content_changes({make_msg(10), make_msg(20), make_msg(30)}));
}

TEST_F(MessageModelTest, HasContentChangesReturnsTrueForPinnedChange) {
  auto msg = make_msg(10);
  msg.pinned = false;
  model_.set_messages({msg});

  auto modified = msg;
  modified.pinned = true;
  EXPECT_TRUE(model_.has_content_changes({modified}));
}

// =============================================================================
// Tier 3: has_content_changes unhinged scenarios
// =============================================================================

TEST_F(MessageModelTest, HasContentChangesReturnsFalseForBothEmpty) {
  // Empty model, empty incoming vector: no changes.
  EXPECT_FALSE(model_.has_content_changes({}));
}

TEST_F(MessageModelTest, HasContentChangesReturnsTrueForSameCountDifferentIds) {
  model_.set_messages({make_msg(10), make_msg(20)});

  // Same count but completely different message IDs
  EXPECT_TRUE(model_.has_content_changes({make_msg(30), make_msg(40)}));
}

TEST_F(MessageModelTest, HasContentChangesReturnsFalseWhenOnlyEmbedProxyUrlsDiffer) {
  // Embed proxy URLs rotate between API responses. The comparison should
  // ignore url/proxy_url on embed images and thumbnails.
  auto msg = make_msg(10);
  kind::Embed embed;
  embed.type = "image";
  embed.title = "Test embed";
  kind::EmbedImage img;
  img.url = "https://images-ext-1.discordapp.net/external/abc/img.png";
  img.proxy_url = "https://images-ext-1.discordapp.net/external/abc/proxy.png";
  img.width = 800;
  img.height = 600;
  embed.image = img;
  kind::EmbedImage thumb;
  thumb.url = "https://images-ext-1.discordapp.net/external/abc/thumb.png";
  thumb.proxy_url = "https://images-ext-1.discordapp.net/external/abc/proxy_thumb.png";
  thumb.width = 128;
  thumb.height = 128;
  embed.thumbnail = thumb;
  msg.embeds.push_back(embed);
  model_.set_messages({msg});

  // Same embed but with rotated proxy URLs
  auto rest_msg = msg;
  rest_msg.embeds[0].image->url = "https://images-ext-1.discordapp.net/external/xyz/img.png";
  rest_msg.embeds[0].image->proxy_url = "https://images-ext-1.discordapp.net/external/xyz/proxy.png";
  rest_msg.embeds[0].thumbnail->url = "https://images-ext-1.discordapp.net/external/xyz/thumb.png";
  rest_msg.embeds[0].thumbnail->proxy_url = "https://images-ext-1.discordapp.net/external/xyz/proxy_thumb.png";
  EXPECT_FALSE(model_.has_content_changes({rest_msg}));
}

TEST_F(MessageModelTest, HasContentChangesReturnsTrueWhenEmbedContentChanges) {
  auto msg = make_msg(10);
  kind::Embed embed;
  embed.type = "rich";
  embed.title = "Original title";
  msg.embeds.push_back(embed);
  model_.set_messages({msg});

  auto modified = msg;
  modified.embeds[0].title = "Changed title";
  EXPECT_TRUE(model_.has_content_changes({modified}));
}

TEST_F(MessageModelTest, HasContentChangesReturnsFalseWhenOnlyReferencedMessageFieldsDiffer) {
  // Simulates the DB round-trip gap: referenced_message_author and
  // referenced_message_content are not stored in the database, so DB-cached
  // messages have them empty while REST messages have them populated.
  auto msg = make_msg(10);
  msg.referenced_message_id = 5;
  // DB-cached version: no author/content for the referenced message
  model_.set_messages({msg});

  // REST version: same message but with referenced_message fields populated
  auto rest_msg = msg;
  rest_msg.referenced_message_author = "someone";
  rest_msg.referenced_message_content = "hello world";
  EXPECT_FALSE(model_.has_content_changes({rest_msg}));
}
