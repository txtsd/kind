#include "cdn_url.hpp"

#include <gtest/gtest.h>
#include <string>

// =============================================================================
// Tier 1: Basic functionality
// =============================================================================

TEST(CdnUrlTest, AssetUrlGetsSizeWithPowerOfTwo) {
  auto result = kind::cdn_url::add_image_size(
      "https://cdn.discordapp.com/icons/123/abc.png", 100);
  EXPECT_EQ(result, "https://cdn.discordapp.com/icons/123/abc.png?size=128");
}

TEST(CdnUrlTest, AttachmentUrlGetsWidthHeightNotSize) {
  auto result = kind::cdn_url::add_image_size(
      "https://cdn.discordapp.com/attachments/111/222/image.png?ex=abc&is=def&hm=ghi", 520);
  // Attachment URLs are signed so they already have ?, separator must be &
  EXPECT_TRUE(result.contains("width=520"));
  EXPECT_TRUE(result.contains("height=520"));
  EXPECT_FALSE(result.contains("size="));
}

TEST(CdnUrlTest, ProxyUrlGetsWidthHeight) {
  auto result = kind::cdn_url::add_image_size(
      "https://media.discordapp.net/attachments/111/222/image.png", 400, 300);
  EXPECT_TRUE(result.contains("width=400"));
  EXPECT_TRUE(result.contains("height=300"));
}

TEST(CdnUrlTest, ExternalProxyUrlGetsWidthHeight) {
  auto result = kind::cdn_url::add_image_size(
      "https://images-ext-1.discordapp.net/external/abc/https/example.com/img.jpg", 200, 150);
  EXPECT_TRUE(result.contains("width=200"));
  EXPECT_TRUE(result.contains("height=150"));
}

TEST(CdnUrlTest, NonDiscordUrlPassedThrough) {
  std::string url = "https://example.com/image.png";
  auto result = kind::cdn_url::add_image_size(url, 100);
  EXPECT_EQ(result, url);
}

TEST(CdnUrlTest, TwoArgSetsHeightEqualToWidth) {
  auto result = kind::cdn_url::add_image_size(
      "https://media.discordapp.net/img.png", 300);
  EXPECT_TRUE(result.contains("width=300"));
  EXPECT_TRUE(result.contains("height=300"));
}

TEST(CdnUrlTest, ThreeArgSetsExplicitDifferentHeight) {
  auto result = kind::cdn_url::add_image_size(
      "https://media.discordapp.net/img.png", 400, 200);
  EXPECT_TRUE(result.contains("width=400"));
  EXPECT_TRUE(result.contains("height=200"));
}

// =============================================================================
// Tier 2: Edge cases
// =============================================================================

TEST(CdnUrlEdgeTest, StickerUrlGetsSizeNotWidthHeight) {
  auto result = kind::cdn_url::add_image_size(
      "https://cdn.discordapp.com/stickers/12345.png", 160);
  EXPECT_TRUE(result.contains("size="));
  EXPECT_FALSE(result.contains("width="));
}

TEST(CdnUrlEdgeTest, SizeClampedAt4096ForLargeValues) {
  auto result = kind::cdn_url::add_image_size(
      "https://cdn.discordapp.com/icons/123/abc.png", 10000);
  EXPECT_EQ(result, "https://cdn.discordapp.com/icons/123/abc.png?size=4096");
}

TEST(CdnUrlEdgeTest, SizeMinimumIs16ForTinyValues) {
  auto result = kind::cdn_url::add_image_size(
      "https://cdn.discordapp.com/icons/123/abc.png", 1);
  EXPECT_EQ(result, "https://cdn.discordapp.com/icons/123/abc.png?size=16");
}

TEST(CdnUrlEdgeTest, AttachmentWithExistingQueryUsesAmpersand) {
  auto result = kind::cdn_url::add_image_size(
      "https://cdn.discordapp.com/attachments/1/2/img.png?ex=abc&is=def&hm=ghi", 300, 200);
  // Must use & as separator since ? already exists, no double &&
  EXPECT_TRUE(result.contains("&width=300"));
  EXPECT_FALSE(result.contains("&&"));
}

TEST(CdnUrlEdgeTest, AttachmentWithTrailingAmpersandNoDuplicate) {
  auto result = kind::cdn_url::add_image_size(
      "https://cdn.discordapp.com/attachments/1/2/img.png?ex=abc&is=def&hm=ghi&", 300, 200);
  EXPECT_TRUE(result.contains("width=300"));
  EXPECT_FALSE(result.contains("&&"));
}

TEST(CdnUrlEdgeTest, ProxyWithTrailingQuestionMark) {
  auto result = kind::cdn_url::add_image_size(
      "https://media.discordapp.net/img.png?", 300, 200);
  EXPECT_TRUE(result.contains("width=300"));
  EXPECT_FALSE(result.contains("??"));
}

TEST(CdnUrlEdgeTest, EmptyUrlReturnsEmpty) {
  auto result = kind::cdn_url::add_image_size("", 100);
  EXPECT_EQ(result, "");
}

TEST(CdnUrlEdgeTest, AssetUrlWithoutQueryUsesQuestionMark) {
  auto result = kind::cdn_url::add_image_size(
      "https://cdn.discordapp.com/avatars/123/abc.png", 64);
  EXPECT_TRUE(result.contains("?size=64"));
}

TEST(CdnUrlEdgeTest, DiscordComProxyUrl) {
  auto result = kind::cdn_url::add_image_size(
      "https://images.discord.com/external/abc/https/example.com/img.jpg", 250, 180);
  EXPECT_TRUE(result.contains("width=250"));
  EXPECT_TRUE(result.contains("height=180"));
}

// =============================================================================
// Tier 3: Unhinged scenarios
// =============================================================================

TEST(CdnUrlUnhingedTest, ZeroWidthGivesSize16) {
  auto result = kind::cdn_url::add_image_size(
      "https://cdn.discordapp.com/icons/123/abc.png", 0);
  EXPECT_EQ(result, "https://cdn.discordapp.com/icons/123/abc.png?size=16");
}

TEST(CdnUrlUnhingedTest, NegativeWidthGivesSize16) {
  auto result = kind::cdn_url::add_image_size(
      "https://cdn.discordapp.com/icons/123/abc.png", -50);
  EXPECT_EQ(result, "https://cdn.discordapp.com/icons/123/abc.png?size=16");
}

TEST(CdnUrlUnhingedTest, VeryLongUrlDoesNotCrash) {
  std::string long_url = "https://cdn.discordapp.com/icons/123/";
  long_url += std::string(100000, 'a');
  long_url += ".png";
  auto result = kind::cdn_url::add_image_size(long_url, 256);
  EXPECT_TRUE(result.contains("size=256"));
  EXPECT_GT(result.size(), long_url.size());
}

TEST(CdnUrlUnhingedTest, AttachmentInSubdomain) {
  // URL that contains "attachments" in the path but is actually on cdn
  auto result = kind::cdn_url::add_image_size(
      "https://cdn.discordapp.com/attachments/999/888/giant.webp?ex=abc", 1920, 1080);
  EXPECT_TRUE(result.contains("width=1920"));
  EXPECT_TRUE(result.contains("height=1080"));
  EXPECT_FALSE(result.contains("size="));
}

TEST(CdnUrlUnhingedTest, ExactPowerOfTwoWidthIsNotDoubled) {
  auto result = kind::cdn_url::add_image_size(
      "https://cdn.discordapp.com/icons/1/a.png", 256);
  EXPECT_EQ(result, "https://cdn.discordapp.com/icons/1/a.png?size=256");
}

TEST(CdnUrlUnhingedTest, Width4096GivesExactly4096) {
  auto result = kind::cdn_url::add_image_size(
      "https://cdn.discordapp.com/icons/1/a.png", 4096);
  EXPECT_EQ(result, "https://cdn.discordapp.com/icons/1/a.png?size=4096");
}

TEST(CdnUrlUnhingedTest, Width4097ClampedTo4096) {
  // 4097 exceeds 4096, but the loop stops at 4096
  auto result = kind::cdn_url::add_image_size(
      "https://cdn.discordapp.com/icons/1/a.png", 4097);
  EXPECT_EQ(result, "https://cdn.discordapp.com/icons/1/a.png?size=4096");
}

TEST(CdnUrlUnhingedTest, NegativeWidthOnProxyUrlClampedTo1) {
  auto result = kind::cdn_url::add_image_size(
      "https://media.discordapp.net/img.png", -50);
  EXPECT_TRUE(result.contains("width=1"));
  EXPECT_TRUE(result.contains("height=1"));
}

TEST(CdnUrlUnhingedTest, NegativeWidthOnAttachmentUrlClampedTo1) {
  auto result = kind::cdn_url::add_image_size(
      "https://cdn.discordapp.com/attachments/1/2/img.png?ex=abc", -50);
  EXPECT_TRUE(result.contains("width=1"));
  EXPECT_TRUE(result.contains("height=1"));
}
