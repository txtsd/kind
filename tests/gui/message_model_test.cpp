#include "models/message_model.hpp"

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
