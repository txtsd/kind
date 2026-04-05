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

// =============================================================================
// constrain_dimensions tests
// =============================================================================

// =============================================================================
// Tier 1: Basic functionality
// =============================================================================

TEST(ConstrainDimensionsTest, NoScalingWhenWithinBounds) {
  auto [w, h] = kind::cdn_url::constrain_dimensions(400, 300, 520, 520);
  EXPECT_EQ(w, 400);
  EXPECT_EQ(h, 300);
}

TEST(ConstrainDimensionsTest, ScalesDownWidthPreservingAspectRatio) {
  // 1920x1080 capped at 520 wide: 520 x 293
  auto [w, h] = kind::cdn_url::constrain_dimensions(1920, 1080, 520, 520);
  EXPECT_EQ(w, 520);
  EXPECT_EQ(h, 292); // 1080 * 520 / 1920 = 292.5 -> truncated to 292
}

TEST(ConstrainDimensionsTest, ScalesDownHeightPreservingAspectRatio) {
  // 400x2000 capped at 520x520: width first: fine (400 <= 520), height: 2000 > 520
  // -> w = 400 * 520 / 2000 = 104, h = 520
  auto [w, h] = kind::cdn_url::constrain_dimensions(400, 2000, 520, 520);
  EXPECT_EQ(w, 104);
  EXPECT_EQ(h, 520);
}

TEST(ConstrainDimensionsTest, VideoMaxHeightCap) {
  // 1920x1080 capped at 520x300 (video): width first -> 520x292, height fine
  auto [w, h] = kind::cdn_url::constrain_dimensions(1920, 1080, 520, 300);
  EXPECT_EQ(w, 520);
  EXPECT_EQ(h, 292);
}

TEST(ConstrainDimensionsTest, TallVideoScaledByHeight) {
  // 400x800 capped at 520x300: width fine, height 800 > 300 -> w = 400*300/800 = 150
  auto [w, h] = kind::cdn_url::constrain_dimensions(400, 800, 520, 300);
  EXPECT_EQ(w, 150);
  EXPECT_EQ(h, 300);
}

// =============================================================================
// Tier 2: Edge cases
// =============================================================================

TEST(ConstrainDimensionsEdgeTest, ExactMaxReturnsUnchanged) {
  auto [w, h] = kind::cdn_url::constrain_dimensions(520, 520, 520, 520);
  EXPECT_EQ(w, 520);
  EXPECT_EQ(h, 520);
}

TEST(ConstrainDimensionsEdgeTest, SquareImageStaysSquare) {
  auto [w, h] = kind::cdn_url::constrain_dimensions(1000, 1000, 520, 520);
  EXPECT_EQ(w, 520);
  EXPECT_EQ(h, 520);
}

TEST(ConstrainDimensionsEdgeTest, VeryWideImageMinHeightClampedTo1) {
  // 10000x1 capped at 520: h = 1*520/10000 = 0 -> clamped to 1
  auto [w, h] = kind::cdn_url::constrain_dimensions(10000, 1, 520, 520);
  EXPECT_EQ(w, 520);
  EXPECT_GE(h, 1);
}

// =============================================================================
// Tier 3: Unhinged scenarios
// =============================================================================

TEST(ConstrainDimensionsUnhingedTest, ZeroDimensionsClampedTo1) {
  auto [w, h] = kind::cdn_url::constrain_dimensions(0, 0, 520, 520);
  EXPECT_GE(w, 1);
  EXPECT_GE(h, 1);
}

TEST(ConstrainDimensionsUnhingedTest, NegativeDimensionsClampedTo1) {
  auto [w, h] = kind::cdn_url::constrain_dimensions(-100, -50, 520, 520);
  EXPECT_GE(w, 1);
  EXPECT_GE(h, 1);
}

TEST(ConstrainDimensionsUnhingedTest, ExtremelyLargeValues) {
  auto [w, h] = kind::cdn_url::constrain_dimensions(100000, 50000, 520, 520);
  EXPECT_LE(w, 520);
  EXPECT_LE(h, 520);
  EXPECT_GE(w, 1);
  EXPECT_GE(h, 1);
}

TEST(ConstrainDimensionsUnhingedTest, BothDimensionsExceedDifferentMaxes) {
  // 800x600 with max 400x300: width caps first -> 400x300, then height is exactly 300
  auto [w, h] = kind::cdn_url::constrain_dimensions(800, 600, 400, 300);
  EXPECT_LE(w, 400);
  EXPECT_LE(h, 300);
}

// =============================================================================
// normalize_cache_key tests
// =============================================================================

// =============================================================================
// Tier 1: Basic functionality
// =============================================================================

TEST(CdnUrlTest, NormalizeCacheKeyStripsSignatureKeepsSizeParams) {
  auto result = kind::cdn_url::normalize_cache_key(
      "https://cdn.discordapp.com/attachments/111/222/image.png"
      "?ex=abc123&is=def456&hm=ghi789&width=520&height=400");
  EXPECT_EQ(result,
      "https://cdn.discordapp.com/attachments/111/222/image.png"
      "?width=520&height=400");
}

TEST(CdnUrlTest, NormalizeCacheKeyNonAttachmentCdnUrlUnchanged) {
  std::string url = "https://cdn.discordapp.com/icons/123/abc.png?size=128";
  EXPECT_EQ(kind::cdn_url::normalize_cache_key(url), url);
}

TEST(CdnUrlTest, NormalizeCacheKeyProxyAttachmentWithOnlyContentParams) {
  std::string url = "https://media.discordapp.net/attachments/111/222/image.png"
                    "?width=400&height=300";
  // Discord proxy with /attachments/ is normalized, but only has content params so unchanged
  auto result = kind::cdn_url::normalize_cache_key(url);
  EXPECT_EQ(result, url);
}

TEST(CdnUrlTest, NormalizeCacheKeyExternalUrlUnchanged) {
  std::string url = "https://example.com/image.png?token=abc123";
  EXPECT_EQ(kind::cdn_url::normalize_cache_key(url), url);
}

// =============================================================================
// Tier 2: Edge cases
// =============================================================================

TEST(CdnUrlEdgeTest, NormalizeCacheKeyNoQueryParamsUnchanged) {
  std::string url = "https://cdn.discordapp.com/attachments/111/222/image.png";
  EXPECT_EQ(kind::cdn_url::normalize_cache_key(url), url);
}

TEST(CdnUrlEdgeTest, NormalizeCacheKeyOnlySignatureParamsReturnsPathOnly) {
  auto result = kind::cdn_url::normalize_cache_key(
      "https://cdn.discordapp.com/attachments/111/222/image.png"
      "?ex=abc123&is=def456&hm=ghi789");
  EXPECT_EQ(result,
      "https://cdn.discordapp.com/attachments/111/222/image.png");
}

TEST(CdnUrlEdgeTest, NormalizeCacheKeyFormatParamPreserved) {
  auto result = kind::cdn_url::normalize_cache_key(
      "https://cdn.discordapp.com/attachments/111/222/image.png"
      "?ex=abc&is=def&hm=ghi&format=webp&width=300");
  EXPECT_EQ(result,
      "https://cdn.discordapp.com/attachments/111/222/image.png"
      "?format=webp&width=300");
}

TEST(CdnUrlEdgeTest, NormalizeCacheKeySameAttachmentDifferentSignaturesSameKey) {
  auto key1 = kind::cdn_url::normalize_cache_key(
      "https://cdn.discordapp.com/attachments/111/222/image.png"
      "?ex=aaa111&is=bbb222&hm=ccc333&width=400&height=300");
  auto key2 = kind::cdn_url::normalize_cache_key(
      "https://cdn.discordapp.com/attachments/111/222/image.png"
      "?ex=xxx999&is=yyy888&hm=zzz777&width=400&height=300");
  EXPECT_EQ(key1, key2);
}

// =============================================================================
// Tier 3: Unhinged scenarios
// =============================================================================

TEST(CdnUrlUnhingedTest, NormalizeCacheKeyEmptyStringReturnsEmpty) {
  EXPECT_EQ(kind::cdn_url::normalize_cache_key(""), "");
}

TEST(CdnUrlUnhingedTest, NormalizeCacheKeyVeryLongAttachmentUrl) {
  std::string url = "https://cdn.discordapp.com/attachments/111/222/";
  url += std::string(100000, 'x');
  url += ".png?ex=abc&is=def&hm=ghi&width=1920&height=1080";
  auto result = kind::cdn_url::normalize_cache_key(url);
  EXPECT_TRUE(result.contains("width=1920"));
  EXPECT_TRUE(result.contains("height=1080"));
  EXPECT_FALSE(result.contains("ex="));
  EXPECT_FALSE(result.contains("is="));
  EXPECT_FALSE(result.contains("hm="));
}

TEST(CdnUrlUnhingedTest, NormalizeCacheKeyNonDiscordAttachmentsPathUnchanged) {
  // Non-Discord URL with "attachments" in path should NOT be normalized
  std::string url = "https://weird.example.com/foo/attachments/bar/image.png"
                    "?ex=abc&is=def&hm=ghi&width=640";
  EXPECT_EQ(kind::cdn_url::normalize_cache_key(url), url);
}
