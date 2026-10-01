#include "mctest.h"
#include "McAtlasData.h"
#include <string.h>

using namespace Mc;

static const char *kManifest =
	"{\"latest\":{\"release\":\"1.21.4\",\"snapshot\":\"25w01a\"},\"versions\":["
	"{\"id\":\"25w01a\",\"type\":\"snapshot\",\"url\":\"https://piston-meta.mojang.com/v1/packages/aaa/25w01a.json\",\"time\":\"t\"},"
	"{\"id\": \"1.21.4\", \"type\": \"release\", \"url\": \"https://piston-meta.mojang.com/v1/packages/bbb/1.21.4.json\", \"time\": \"t\"}]}";

static const char *kVersion =
	"{\"downloads\":{\"client\":{\"sha1\":\"x\",\"size\":1,\"url\":\"https://piston-data.mojang.com/v1/objects/abc/client.jar\"},"
	"\"client_mappings\":{\"url\":\"https://piston-data.mojang.com/v1/objects/zzz/client.txt\"},"
	"\"server\":{\"url\":\"https://piston-data.mojang.com/v1/objects/s/server.jar\"}}}";

MC_TEST(atlas_json_find_string_basic_and_whitespace)
{
	std::string v;
	size_t kp = 0, af = 0;
	MC_CHECK(JsonFindString("{ \"a\" :  \"hello\" }", "a", 0, v, &kp, &af));
	MC_CHECK(v == "hello");
	MC_CHECK_EQ(kp, 2);
	MC_CHECK(!JsonFindString("{\"a\":\"x\"}", "b", 0, v));
}

MC_TEST(atlas_json_find_string_skips_non_string_values)
{
	std::string v;
	MC_CHECK(JsonFindString("{\"k\":{\"x\":1},\"k\":\"second\"}", "k", 0, v));
	MC_CHECK(v == "second");
	MC_CHECK(!JsonFindString("{\"k\":5}", "k", 0, v));
}

MC_TEST(atlas_json_find_string_rejects_backslash_values)
{
	std::string v;
	MC_CHECK(!JsonFindString("{\"k\":\"a\\\"b\"}", "k", 0, v));
}

MC_TEST(atlas_latest_release)
{
	std::string id;
	MC_CHECK(ExtractLatestRelease(kManifest, id));
	MC_CHECK(id == "1.21.4");
	MC_CHECK(!ExtractLatestRelease("{\"versions\":[]}", id));
}

MC_TEST(atlas_version_url_picks_the_matching_entry)
{
	std::string url;
	MC_CHECK(ExtractVersionUrl(kManifest, "1.21.4", url));
	MC_CHECK(url == "https://piston-meta.mojang.com/v1/packages/bbb/1.21.4.json");
	MC_CHECK(ExtractVersionUrl(kManifest, "25w01a", url));
	MC_CHECK(url == "https://piston-meta.mojang.com/v1/packages/aaa/25w01a.json");
	MC_CHECK(!ExtractVersionUrl(kManifest, "9.9", url));
}

MC_TEST(atlas_version_url_does_not_borrow_the_next_entries_url)
{
	std::string url;
	// the entry for "a" has no url; the next entry "b" has one
	const char *m = "{\"versions\":[{\"id\":\"a\",\"type\":\"release\"},{\"id\":\"b\",\"url\":\"https://piston-meta.mojang.com/x.json\"}]}";
	MC_CHECK(!ExtractVersionUrl(m, "a", url));
	MC_CHECK(ExtractVersionUrl(m, "b", url));
}

MC_TEST(atlas_client_url_is_not_the_mappings_url)
{
	std::string url;
	MC_CHECK(ExtractClientUrl(kVersion, url));
	MC_CHECK(url == "https://piston-data.mojang.com/v1/objects/abc/client.jar");
	MC_CHECK(!ExtractClientUrl("{\"downloads\":{\"client_mappings\":{\"url\":\"https://piston-data.mojang.com/z\"}}}", url));
	MC_CHECK(!ExtractClientUrl("{}", url));
}

MC_TEST(atlas_allowed_urls)
{
	MC_CHECK(IsAllowedMojangUrl("https://piston-data.mojang.com/v1/objects/abc/client.jar"));
	MC_CHECK(IsAllowedMojangUrl("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"));
	MC_CHECK(IsAllowedMojangUrl("https://launcher.mojang.com/v1/objects/a/b.jar"));
	MC_CHECK(IsAllowedMojangUrl("https://launchermeta.mojang.com/mc/game/x.json"));
}

MC_TEST(atlas_rejected_urls)
{
	MC_CHECK(!IsAllowedMojangUrl(""));
	MC_CHECK(!IsAllowedMojangUrl("http://piston-data.mojang.com/x"));
	MC_CHECK(!IsAllowedMojangUrl("https://example.com/x"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com.evil.com/x"));
	MC_CHECK(!IsAllowedMojangUrl("https://evil.com/piston-data.mojang.com/x"));
	MC_CHECK(!IsAllowedMojangUrl("https://user@piston-data.mojang.com/x"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com/"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com/a b"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com/a;rm -rf"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com/a\"b"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com/a`b`"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com/a$(x)"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com/a\nb"));
	MC_CHECK(!IsAllowedMojangUrl(std::string("https://piston-data.mojang.com/") + std::string(600, 'a')));
}

MC_TEST(atlas_block_texture_files)
{
	MC_CHECK(BlockTextureFile(BLOCK_AIR) == nullptr);
	MC_CHECK(strcmp(BlockTextureFile(BLOCK_DIRT), "dirt.png") == 0);
	MC_CHECK(strcmp(BlockTextureFile(BLOCK_STONE), "stone.png") == 0);
	MC_CHECK(strcmp(BlockTextureFile(BLOCK_WOOD), "oak_planks.png") == 0);
	MC_CHECK(strcmp(BlockTextureFile(BLOCK_GLASS), "glass.png") == 0);
	MC_CHECK(BlockTextureFile(200) == nullptr);
}

static void FillTile(TilePixels &t, uint8_t v)
{
	t.valid = true;
	for(int i = 0; i < TILE_PIXELS * TILE_PIXELS * 4; i++)
		t.rgba[i] = v;
}

MC_TEST(atlas_compose_places_tiles_and_falls_back_to_flat_colours)
{
	TilePixels tiles[ATLAS_TILES * ATLAS_TILES];
	for(int i = 0; i < ATLAS_TILES * ATLAS_TILES; i++){
		tiles[i].valid = false;
		memset(tiles[i].rgba, 0, sizeof(tiles[i].rgba));
	}
	FillTile(tiles[1], 77);			// dirt tile valid, all bytes 77
	// mark one distinctive pixel in tile 5 (tx=1, ty=1): local (3,2) = 1,2,3,4
	tiles[5].valid = true;
	uint8_t *p = &tiles[5].rgba[(2 * TILE_PIXELS + 3) * 4];
	p[0] = 1; p[1] = 2; p[2] = 3; p[3] = 4;

	std::vector<uint8_t> out;
	ComposeAtlas(tiles, out);
	MC_CHECK_EQ(out.size(), ATLAS_PIXELS * ATLAS_PIXELS * 4);

	// tile 1 = (tx=1, ty=0): pixel (16,0)
	MC_CHECK_EQ(out[(0 * ATLAS_PIXELS + 16) * 4], 77);
	// tile 5 distinctive pixel at atlas (16+3, 16+2)
	const uint8_t *q = &out[((16 + 2) * ATLAS_PIXELS + (16 + 3)) * 4];
	MC_CHECK_EQ(q[0], 1); MC_CHECK_EQ(q[1], 2); MC_CHECK_EQ(q[2], 3); MC_CHECK_EQ(q[3], 4);
	// tile 2 (stone, tx=2, ty=0) is invalid: flat colour of stone, alpha 255
	const uint8_t *s = &out[(0 * ATLAS_PIXELS + 32) * 4];
	MC_CHECK_EQ(s[0], GetBlockInfo(BLOCK_STONE).r);
	MC_CHECK_EQ(s[1], GetBlockInfo(BLOCK_STONE).g);
	MC_CHECK_EQ(s[2], GetBlockInfo(BLOCK_STONE).b);
	MC_CHECK_EQ(s[3], 255);
	// tile 15 (id >= BLOCK_COUNT) is invalid: magenta
	const uint8_t *m = &out[((3 * 16) * ATLAS_PIXELS + 3 * 16) * 4];
	MC_CHECK_EQ(m[0], 255); MC_CHECK_EQ(m[1], 0); MC_CHECK_EQ(m[2], 255); MC_CHECK_EQ(m[3], 255);
}
