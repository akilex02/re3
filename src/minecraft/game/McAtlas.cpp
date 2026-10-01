#include "common.h"

#ifdef MINECRAFT_MODE

#include "McAtlas.h"
#include "McAtlasData.h"
#include "McItemTable.h"
#include <atomic>
#include <thread>
#include <system_error>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

// Everything below is plain old data with constant initialisation: the detached download thread may
// still be running while the process exits, so it must never touch an object with a destructor.
static const char assetDir[] = "mcassets";
static const char manifestUrl[] = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";
static const char manifestFile[] = "mcassets/manifest.json";
static const char versionFile[] = "mcassets/version.json";
static const char jarFile[] = "mcassets/client.jar";
static const char jarTextureDir[] = "assets/minecraft/textures/";
static const long maxJsonSize = 16 * 1024 * 1024;
static const long maxPngSize = 1024 * 1024;

static_assert(Mc::ITEM_COUNT <= Mc::ATLAS_TILES * Mc::ATLAS_TILES, "every item id needs an atlas tile");

// Shared with the download thread. A session number identifies one Init; Init and Shutdown bump the
// counter, which is the stop signal: a thread whose session is no longer current aborts at its next step
// and its result is ignored. Session 0 is never used.
static std::atomic<uint32> sessionCounter(0);
static std::atomic<uint32> readySession(0);
static std::atomic<uint32> failedSession(0);
static std::atomic<bool> threadLaunched(false);

// Main thread only.
static uint32 currentSession;
static bool waitingForDownload;
static RwTexture *atlasTexture;
static uint32 generation;

namespace McAtlas
{

static long
FileSize(const char *path)
{
	FILE *f = fopen(path, "rb");
	if(f == nil)
		return -1;
	long size = -1;
	if(fseek(f, 0, SEEK_END) == 0)
		size = ftell(f);
	fclose(f);
	return size;
}

// Local path of the extracted texture of an item id (unzip -j flattens the jar directories).
static std::string
LocalTexturePath(uint8 id)
{
	return std::string(assetDir) + "/" + Mc::TextureBasename(Mc::BlockTextureFile(id));
}

static bool
CacheComplete(void)
{
	for(int id = 1; id < Mc::ITEM_COUNT; id++)
		if(FileSize(LocalTexturePath(id).c_str()) <= 0)
			return false;
	return true;
}

// ---- download thread (no librw here) ----

static bool
Stopped(uint32 session)
{
	return sessionCounter.load() != session;
}

// Exit status of the command, or -1 if it could not be run or did not exit normally.
static int
RunCommand(const std::string &cmd)
{
	int status = system(cmd.c_str());
#ifdef _WIN32
	return status;
#else
	if(status == -1 || !WIFEXITED(status))
		return -1;
	return WEXITSTATUS(status);
#endif
}

static bool
MakeAssetDir(void)
{
#ifdef _WIN32
	int res = _mkdir(assetDir);
#else
	int res = mkdir(assetDir, 0755);
#endif
	return res == 0 || errno == EEXIST;
}

static bool
ReadJsonFile(const char *path, std::string &out)
{
	long size = FileSize(path);
	if(size <= 0 || size > maxJsonSize)
		return false;
	FILE *f = fopen(path, "rb");
	if(f == nil)
		return false;
	out.resize(size);
	size_t n = fread(&out[0], 1, size, f);
	fclose(f);
	return n == (size_t)size;
}

static void
RemoveTemporaryFiles(void)
{
	remove(manifestFile);
	remove(versionFile);
}

static bool
Fail(const char *step, const char *why)
{
	printf("McAtlas: download failed at step '%s' (%s); using flat colours\n", step, why);
	return false;
}

static bool
FailCommand(const char *step, const char *program, int status)
{
	printf("McAtlas: download failed at step '%s' (%s returned %d); using flat colours\n", step, program, status);
	return false;
}

// Only prints the release id when it looks like one; it comes from the network.
static const char*
PrintableId(const std::string &id)
{
	if(id.empty() || id.size() > 32)
		return "?";
	for(size_t i = 0; i < id.size(); i++){
		char c = id[i];
		if(!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9') && c != '.' && c != '-' && c != '_')
			return "?";
	}
	return id.c_str();
}

// Extracts every block and item texture from the client jar and returns how many exist afterwards.
// Entries missing from the jar are tolerated (their tile keeps its flat colour); zero means the jar is
// unusable. unzip returns 1 for warnings and 11 when a named entry is missing, so the files decide.
static int
ExtractTextures(void)
{
	std::string cmd = std::string("unzip -o -j -q \"") + jarFile + "\"";
	for(int id = 1; id < Mc::ITEM_COUNT; id++)
		cmd += std::string(" \"") + jarTextureDir + Mc::BlockTextureFile(id) + "\"";
	cmd += std::string(" -d \"") + assetDir + "\"";
	RunCommand(cmd);
	int n = 0;
	for(int id = 1; id < Mc::ITEM_COUNT; id++)
		if(FileSize(LocalTexturePath(id).c_str()) > 0)
			n++;
	return n;
}

// Every string reaching the shell is built from the constants above plus URLs that passed
// Mc::IsAllowedMojangUrl (which allows no quote, space or shell metacharacter).
static bool
Download(uint32 session)
{
	int status;
	std::string cmd, json, id, url;

	if(Stopped(session))
		return false;
	if(!MakeAssetDir())
		return Fail("create mcassets", strerror(errno));

	// the client jar is kept after the first download: extract from it, download only when it is missing or unusable
	if(FileSize(jarFile) > 0){
		printf("McAtlas: extracting textures from the existing client jar\n");
		if(ExtractTextures() > 0)
			return true;
		printf("McAtlas: existing client jar is unusable; downloading it again\n");
		remove(jarFile);
		if(Stopped(session))
			return false;
	}

	// manifest
	if(!Mc::IsAllowedMojangUrl(manifestUrl))
		return Fail("manifest", "manifest URL rejected");
	printf("McAtlas: downloading version manifest\n");
	cmd = std::string("curl -fsSL --proto =https --proto-redir =https --connect-timeout 10 --max-time 60 -o \"") + manifestFile + "\" \"" + manifestUrl + "\"";
	status = RunCommand(cmd);
	if(status != 0)
		return FailCommand("manifest", "curl", status);
	if(Stopped(session))
		return false;

	// version json
	if(!ReadJsonFile(manifestFile, json))
		return Fail("manifest", "cannot read mcassets/manifest.json");
	if(!Mc::ExtractLatestRelease(json, id))
		return Fail("manifest", "no latest release");
	if(!Mc::ExtractVersionUrl(json, id, url))
		return Fail("version url", "release not listed");
	if(!Mc::IsAllowedMojangUrl(url))
		return Fail("version url", "URL rejected");
	printf("McAtlas: latest release %s, downloading version info\n", PrintableId(id));
	cmd = std::string("curl -fsSL --proto =https --proto-redir =https --connect-timeout 10 --max-time 60 -o \"") + versionFile + "\" \"" + url + "\"";
	status = RunCommand(cmd);
	if(status != 0)
		return FailCommand("version json", "curl", status);
	if(Stopped(session))
		return false;

	// client jar
	if(!ReadJsonFile(versionFile, json))
		return Fail("version json", "cannot read mcassets/version.json");
	if(!Mc::ExtractClientUrl(json, url))
		return Fail("client url", "no client download");
	if(!Mc::IsAllowedMojangUrl(url))
		return Fail("client url", "URL rejected");
	printf("McAtlas: downloading client jar\n");
	cmd = std::string("curl -fsSL --proto =https --proto-redir =https --connect-timeout 10 --max-time 120 -o \"") + jarFile + "\" \"" + url + "\"";
	status = RunCommand(cmd);
	if(status != 0)
		return FailCommand("client jar", "curl", status);
	if(Stopped(session))
		return false;

	if(ExtractTextures() == 0){
		remove(jarFile);	// truncated or not a jar
		return Fail("extract textures", "no texture could be extracted from the client jar");
	}
	return true;
}

static void
DownloadThread(uint32 session)
{
	bool ok = Download(session);
	RemoveTemporaryFiles();
	if(Stopped(session))
		return;
	if(ok){
		printf("McAtlas: download complete\n");
		readySession.store(session);
	}else
		failedSession.store(session);
}

// ---- loading (main thread) ----

static bool
LoadTile(uint8 id, Mc::TilePixels &tile, bool &loggedCrop)
{
	std::string path = LocalTexturePath(id);
	tile.valid = false;

	// rw::readPNG asserts when it cannot open the file
	long size = FileSize(path.c_str());
	if(size <= 0 || size >= maxPngSize){
		printf("McAtlas: %s missing, empty or too large\n", path.c_str());
		return false;
	}
	rw::Image *img = rw::readPNG(path.c_str());
	if(img == nil){
		printf("McAtlas: %s is not a supported PNG\n", path.c_str());
		return false;
	}
	if(img->width < Mc::TILE_PIXELS || img->height < Mc::TILE_PIXELS){
		printf("McAtlas: %s is %dx%d, smaller than %dx%d\n", path.c_str(), img->width, img->height,
			Mc::TILE_PIXELS, Mc::TILE_PIXELS);
		img->destroy();
		return false;
	}
	bool paletted = img->depth == 8 || img->depth == 4;
	if((img->depth != 32 && img->depth != 24 && !paletted) || (paletted && img->palette == nil)){
		printf("McAtlas: %s has unsupported depth %d\n", path.c_str(), img->depth);
		img->destroy();
		return false;
	}
	if((img->width > Mc::TILE_PIXELS || img->height > Mc::TILE_PIXELS) && !loggedCrop){
		printf("McAtlas: %s is %dx%d, using its top-left %dx%d\n", path.c_str(), img->width, img->height,
			Mc::TILE_PIXELS, Mc::TILE_PIXELS);
		loggedCrop = true;
	}

	// librw images: 32 bit is RGBA, 24 bit RGB, 4 and 8 bit one palette index per byte (RGBA palette); row 0 on top
	for(int y = 0; y < Mc::TILE_PIXELS; y++){
		const uint8 *row = img->pixels + y * img->stride;
		for(int x = 0; x < Mc::TILE_PIXELS; x++){
			uint8 *dst = &tile.rgba[(y * Mc::TILE_PIXELS + x) * 4];
			switch(img->depth){
			case 32:
				memcpy(dst, row + x * 4, 4);
				break;
			case 24:
				memcpy(dst, row + x * 3, 3);
				dst[3] = 255;
				break;
			default:
				if(row[x] >= (1 << img->depth)){
					printf("McAtlas: %s has a palette index out of range\n", path.c_str());
					img->destroy();
					return false;
				}
				memcpy(dst, img->palette + row[x] * 4, 4);
				break;
			}
		}
	}
	img->destroy();
	tile.valid = true;
	return true;
}

static RwTexture*
CreateTexture(const std::vector<uint8_t> &atlas)
{
	RwImage *img = RwImageCreate(Mc::ATLAS_PIXELS, Mc::ATLAS_PIXELS, 32);
	if(img == nil)
		return nil;
	RwImageAllocatePixels(img);
	uint8 *pixels = RwImageGetPixels(img);
	int32 stride = RwImageGetStride(img);
	for(int y = 0; y < Mc::ATLAS_PIXELS; y++)
		memcpy(pixels + y * stride, &atlas[y * Mc::ATLAS_PIXELS * 4], Mc::ATLAS_PIXELS * 4);

	// no MIPMAP flag in the format: a single level
	RwInt32 width, height, depth, format;
	RwRaster *raster = nil;
	if(RwImageFindRasterFormat(img, rwRASTERTYPETEXTURE, &width, &height, &depth, &format)){
		raster = RwRasterCreate(width, height, depth, format);
		if(raster && RwRasterSetFromImage(raster, img) == nil){
			RwRasterDestroy(raster);
			raster = nil;
		}
	}
	RwImageDestroy(img);
	if(raster == nil)
		return nil;

	RwTexture *tex = RwTextureCreate(raster);
	if(tex == nil){
		RwRasterDestroy(raster);
		return nil;
	}
	RwTextureSetFilterMode(tex, rwFILTERNEAREST);
	RwTextureSetAddressing(tex, rwTEXTUREADDRESSCLAMP);
	return tex;
}

static void
ReleaseTexture(void)
{
	if(atlasTexture){
		RwTextureDestroy(atlasTexture);
		atlasTexture = nil;
	}
}

static void
LoadAtlas(void)
{
	Mc::TilePixels tiles[Mc::ATLAS_TILES * Mc::ATLAS_TILES];
	for(int i = 0; i < Mc::ATLAS_TILES * Mc::ATLAS_TILES; i++)
		tiles[i].valid = false;

	int numValid = 0;
	bool loggedCrop = false;
	for(int id = 1; id < Mc::ITEM_COUNT; id++){
		if(LoadTile(id, tiles[id], loggedCrop)){
			numValid++;
			continue;
		}
		// a file that exists but cannot be used is removed so that the next launch downloads it again
		std::string path = LocalTexturePath(id);
		if(FileSize(path.c_str()) >= 0 && remove(path.c_str()) == 0)
			printf("McAtlas: removed broken cached texture %s; it will be downloaded again at the next launch\n", path.c_str());
	}
	if(numValid == 0){
		printf("McAtlas: no usable block texture; using flat colours\n");
		return;
	}

	std::vector<uint8_t> atlas;
	Mc::ComposeAtlas(tiles, atlas);
	RwTexture *tex = CreateTexture(atlas);
	if(tex == nil){
		printf("McAtlas: could not create the atlas texture; using flat colours\n");
		return;
	}
	ReleaseTexture();
	atlasTexture = tex;
	generation++;
	printf("McAtlas: atlas ready (%d of %d textures)\n", numValid, Mc::ITEM_COUNT - 1);
}

void
Init(void)
{
	assert(Mc::IsAllowedMojangUrl(manifestUrl));
	ReleaseTexture();
	waitingForDownload = false;
	currentSession = ++sessionCounter;

	if(CacheComplete()){
		// leftovers of a download whose curl finished after the game had quit
		RemoveTemporaryFiles();
		LoadAtlas();
		return;
	}
	if(threadLaunched.exchange(true)){
		printf("McAtlas: block textures missing and the download already ran this launch; using flat colours\n");
		return;
	}
	try{
		std::thread(DownloadThread, currentSession).detach();
	}catch(const std::system_error &e){
		printf("McAtlas: could not start the download thread (%s); using flat colours\n", e.what());
		return;
	}
	waitingForDownload = true;
}

void
Update(void)
{
	if(!waitingForDownload)
		return;
	if(readySession.load() == currentSession){
		waitingForDownload = false;
		LoadAtlas();
	}else if(failedSession.load() == currentSession)
		waitingForDownload = false;
}

RwTexture*
GetTexture(void)
{
	return atlasTexture;
}

uint32
Generation(void)
{
	return generation;
}

void
Shutdown(void)
{
	++sessionCounter;	// stop signal for a running download
	waitingForDownload = false;
	ReleaseTexture();
}

}

#endif
