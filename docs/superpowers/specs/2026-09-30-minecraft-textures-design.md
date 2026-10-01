# Minecraft mode: real block textures from Mojang — design

Date: 2026-09-30
Status: approved by the user in conversation
Parent: `2026-09-30-minecraft-mode-design.md` (McAtlas, deferred from phase 1).

## Goal

Placed blocks are drawn with the real Minecraft block textures instead of flat colours. The textures are fetched from Mojang on the user's machine; nothing of Mojang's or Rockstar's is redistributed or committed.

## Flow

1. First launch (no cache): a **background thread** (so game loading is not blocked):
   - `curl` the Mojang version manifest (`https://piston-meta.mojang.com/mc/game/version_manifest_v2.json`), read `latest.release`.
   - `curl` that version's JSON (URL from the manifest), read `downloads.client.url`.
   - `curl` the client jar (about 25 MB), `unzip` only the needed PNGs from `assets/minecraft/textures/block/` into `mcassets/` (next to the game, i.e. the working directory), delete the jar.
2. Later launches: PNGs are in `mcassets/`; nothing is downloaded.
3. When the PNGs are available the main thread composes a 64x64 atlas (4x4 tiles of 16x16; tile index = block id, the layout `McMesher` already uses: tile = `(id % 4, id / 4)`), uploads it as a nearest-filtered, clamped texture, and the renderer re-meshes all chunks with `textured = true`.

Block id to file: dirt -> `dirt.png`, stone -> `stone.png`, wood -> `oak_planks.png`, glass -> `glass.png`. Only the four current blocks; adding more is one table line.

## Failure and safety

- Any failure (no network, no `curl` or `unzip`, corrupt download, missing PNG, undecodable PNG) is logged on stdout and the game keeps using flat colours; it never crashes or blocks. A failed attempt is retried at the next launch, not in a loop.
- A tile whose PNG is missing or invalid is filled with that block's flat colour inside the atlas.
- URLs read from the JSON are validated before they reach a shell: `https://`, host exactly one of `piston-meta.mojang.com`, `piston-data.mojang.com`, `launcher.mojang.com`, `launchermeta.mojang.com`, a non-empty path, only characters `[A-Za-z0-9._~:/?&=%+-]`, length < 512, no `@`.
- The downloader thread must never delay quitting the game: it is detached, uses only process-lifetime state (atomics), checks a stop flag between steps, and every `curl` has `--connect-timeout` and `--max-time`.
- librw's `rw::readPNG` asserts when the file is missing: always check the file exists and is non-empty first.
- `mcassets/` is added to `.gitignore`.

## Units

| Unit | Responsibility | Where |
|---|---|---|
| `McAtlasData` | JSON field extraction, URL validation, block id to file name, atlas composition (pure) | core + tests |
| `World::MarkAllDirty` | Re-mesh trigger for atlas arrival | core + test |
| `McAtlas` | Cache check, downloader thread, PNG loading, atlas texture, generation counter | game |
| `McRenderer` (changed) | Binds the atlas, meshes `textured = true`, re-meshes on generation change, restores render state | game |
| `McMode` (changed) | `McAtlas::Init/Update/Shutdown` | game |

## Out of scope

More block types, animated textures, per-face textures (grass top/side), mipmaps, resource packs, an in-game download progress UI.

## Testing

Unit tests for the pure parts. In game: first run prints the download steps and then blocks show Minecraft textures without a restart; second run uses the cache; with the network off the first run keeps flat colours and logs why; deleting `mcassets/` and relaunching re-downloads.
