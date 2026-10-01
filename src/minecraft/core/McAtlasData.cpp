#include "McAtlasData.h"
#include "McItemTable.h"
#include <string.h>

namespace Mc {

bool JsonFindString(const std::string &json, const char *key, size_t from, std::string &value,
	size_t *keyPos, size_t *after)
{
	std::string needle = std::string("\"") + key + "\"";
	size_t pos = from;
	for(;;){
		pos = json.find(needle, pos);
		if(pos == std::string::npos)
			return false;
		size_t i = pos + needle.size();
		while(i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\r' || json[i] == '\n'))
			i++;
		if(i < json.size() && json[i] == ':'){
			i++;
			while(i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\r' || json[i] == '\n'))
				i++;
			if(i < json.size() && json[i] == '"'){
				size_t end = i + 1;
				while(end < json.size() && json[end] != '"'){
					if(json[end] == '\\')
						return false;
					end++;
				}
				if(end >= json.size())
					return false;
				value = json.substr(i + 1, end - i - 1);
				if(keyPos) *keyPos = pos;
				if(after) *after = end + 1;
				return true;
			}
		}
		pos += needle.size();
	}
}

bool ExtractLatestRelease(const std::string &manifest, std::string &id)
{
	size_t l = manifest.find("\"latest\"");
	if(l == std::string::npos)
		return false;
	return JsonFindString(manifest, "release", l, id);
}

bool ExtractVersionUrl(const std::string &manifest, const std::string &id, std::string &url)
{
	size_t pos = 0;
	for(;;){
		std::string v;
		size_t after = 0;
		if(!JsonFindString(manifest, "id", pos, v, nullptr, &after))
			return false;
		if(v == id){
			size_t keyPos = 0;
			if(!JsonFindString(manifest, "url", after, url, &keyPos))
				return false;
			size_t nextId = manifest.find("\"id\"", after);
			if(nextId != std::string::npos && nextId < keyPos)
				return false;	// that url belongs to the next entry
			return true;
		}
		pos = after;
	}
}

bool ExtractClientUrl(const std::string &versionJson, std::string &url)
{
	size_t d = versionJson.find("\"downloads\"");
	if(d == std::string::npos)
		return false;
	size_t c = versionJson.find("\"client\"", d);
	if(c == std::string::npos)
		return false;
	// Find the closing brace of the client object (no nested braces)
	size_t closeBrace = versionJson.find('}', c);
	if(closeBrace == std::string::npos)
		return false;
	// Search for url within the client object
	size_t keyPos = 0;
	if(!JsonFindString(versionJson, "url", c, url, &keyPos))
		return false;
	// Verify the url key belongs to the client object (keyPos < closing brace)
	if(keyPos >= closeBrace)
		return false;
	return true;
}

bool IsAllowedMojangUrl(const std::string &url)
{
	static const char *prefix = "https://";
	static const char *hosts[] = {
		"piston-meta.mojang.com", "piston-data.mojang.com", "launcher.mojang.com", "launchermeta.mojang.com"
	};
	if(url.size() >= 512 || url.compare(0, strlen(prefix), prefix) != 0)
		return false;
	for(size_t i = 0; i < url.size(); i++){
		char c = url[i];
		bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
			c == '.' || c == '_' || c == '~' || c == ':' || c == '/' || c == '?' ||
			c == '=' || c == '+' || c == '-';
		if(!ok)
			return false;
	}
	size_t hostStart = strlen(prefix);
	size_t slash = url.find('/', hostStart);
	if(slash == std::string::npos || slash + 1 >= url.size())
		return false;
	std::string host = url.substr(hostStart, slash - hostStart);
	for(size_t i = 0; i < sizeof(hosts) / sizeof(hosts[0]); i++)
		if(host == hosts[i])
			return true;
	return false;
}

const char *BlockTextureFile(uint8_t id)
{
	return GetItemInfo(id).texture;
}

void ComposeAtlas(const TilePixels tiles[ATLAS_TILES * ATLAS_TILES], std::vector<uint8_t> &out)
{
	out.assign(ATLAS_PIXELS * ATLAS_PIXELS * 4, 0);
	for(int t = 0; t < ATLAS_TILES * ATLAS_TILES; t++){
		int tx = t % ATLAS_TILES, ty = t / ATLAS_TILES;
		for(int y = 0; y < TILE_PIXELS; y++)
		for(int x = 0; x < TILE_PIXELS; x++){
			uint8_t *dst = &out[((ty * TILE_PIXELS + y) * ATLAS_PIXELS + tx * TILE_PIXELS + x) * 4];
			if(tiles[t].valid){
				memcpy(dst, &tiles[t].rgba[(y * TILE_PIXELS + x) * 4], 4);
			}else if(t < ITEM_COUNT){
				const ItemInfo &bi = GetItemInfo((uint8_t)t);
				dst[0] = bi.r; dst[1] = bi.g; dst[2] = bi.b; dst[3] = 255;
			}else{
				dst[0] = 255; dst[1] = 0; dst[2] = 255; dst[3] = 255;
			}
		}
	}
}

}
