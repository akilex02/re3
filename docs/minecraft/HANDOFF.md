# Minecraft mode para re3 (GTA III): estado y traspaso

Documento para retomar el trabajo en otra conversación sin cargar el historial. Última actualización: 2026-09-30 (núcleo de supervivencia añadido; ver sección 9). Rama: `minecraft-mode` (sin fusionar en `master`).

## 1. Qué es

Un "modo Steve" dentro de re3 (port de código abierto de GTA III): con **F8** el jugador pasa a poder colocar y romper bloques voxel en el mundo de GTA, con texturas reales de Minecraft descargadas de Mojang, colisión con bloques para el jugador, peatones y coches, hotbar en pantalla y daño a peatones. Inspirado en SkyCraft (https://github.com/chasmlol/SkyCraft, Minecraft dentro de Skyrim). Aquí se hizo la **versión nativa** (voxels dentro de re3, un solo proceso); el puente con un Minecraft real queda como fase posterior.

## 2. Cómo compilar, probar y ejecutar

Desde `/home/akilex/Descargas/gtas/re3`:

```
# tests puros del núcleo (proyecto CMake aparte, no se compila dentro de re3)
cmake -S tests/minecraft -B build/mctest && cmake --build build/mctest -j && build/mctest/mctests   # 127 tests

# juego (build Ninja ya configurado en build/)
cd build && cmake . && cmake --build . -j$(nproc)     # cmake . es necesario al añadir ficheros (glob)

# ejecutar sin pisar el binario propio del usuario
cd "/mnt/1TB/Juegos/GTAs/0. GTA III/0. TEST" && /home/akilex/Descargas/gtas/re3/build/src/re3
```

- El interruptor es la opción CMake `RE3_MINECRAFT_MODE` (por defecto ON), que define `MINECRAFT_MODE`. **No** está en `src/core/config.h`.
- Controles en modo Steve: F8 activa/desactiva; clic derecho coloca, clic izquierdo rompe o golpea a un peatón; rueda y teclas 1-4 eligen bloque. El clic/rueda solo funcionan además para las acciones normales de GTA si el usuario los mapeó en el menú de controles (ya lo hizo).
- Los PNG de Mojang se guardan en `mcassets/` junto al juego (nunca se versionan). El mundo se guarda en `mcworld.dat` (autoguardado ~45 s y al salir).

## 3. Mapa de código

`src/minecraft/core/` (solo biblioteca estándar, con tests en `tests/minecraft/`):
`McBlocks` (ids, colores), `McWorld` (chunks 16³ dispersos, guardado atómico `MCW1`, `Revision`, `GetBounds`, `MarkAllDirty`), `McRay` (DDA), `McMesher` (mallas con caras ocultas; `ExtractQuads` y `MAX_QUADS_PER_DRAW = 1666` parten cada malla en lotes por el límite de 10000 vértices de librw), `McCollide` (caja vertical vs voxels, `IsStandingOnBlocks`), `McOrientedBox` (caja con yaw, empuje y barrido), `McAtlasData` (JSON de Mojang, validación de URL, atlas 64x64), `McHotbarLayout`.

`src/minecraft/game/` (depende de re3, todo bajo `#ifdef MINECRAFT_MODE`):
`McMode` (alterna F8, ciclo de vida, autoguardado, suelo del jugador), `McRenderer` (RwIm3D por lotes, estados de render restaurados), `McInteract` (rayo, colocar/romper/golpear, teclas, `CanInteract`), `McEntities` (peatones y coches), `McAtlas` (descarga en hilo con `curl`/`unzip`, carga PNG con librw, textura del atlas), `McHotbar` (HUD 2D).

Ganchos en código de Rockstar (todos tras `#ifdef MINECRAFT_MODE`): `src/core/Game.cpp` (Init, ShutDown, Process tras `CWorld::Process()`), `src/core/main.cpp` (`RenderEffects` y `Render2dStuff`). `src/CMakeLists.txt` añade la opción y la definición.

Documentación de diseño: `docs/superpowers/specs/` y `docs/superpowers/plans/` (modo base, colisión de entidades, texturas, hotbar).

## 4. Decisiones y trampas que conviene recordar

- **No tocar ni stagear `src/core/config.h`**: el árbol de trabajo del usuario tiene reformateo suyo sin commitear. La rama no modifica ese archivo.
- librw GL3 (`vendor/librw`, no editar): `setAddressU/V` tiene una condición invertida; por eso el filtro/direccionamiento de la textura se fija con **ningún raster enlazado** (ver `McRenderer.cpp`). Además `rw::readPNG` hace `assert` si el archivo no existe: siempre comprobar existencia y tamaño antes.
- Las URLs que salen del JSON de Mojang solo llegan al shell tras `IsAllowedMojangUrl` (hosts de Mojang, solo `https`, caracteres `[A-Za-z0-9._~:/?=+-]`).
- El jugador en pie sobre voxels: GTA solo limpia `bIsInTheAir` con suelo propio a menos de 1.3, así que `McMode` llama a `SetLanding()` al detectar bloques bajo los pies.
- Vehículos: se empujan fuera de los bloques tras `CWorld::Process` (post-proceso), con un barrido solo contra el efecto túnel y descarte de correcciones > 3.0 (coches nacidos dentro de bloques).
- Entorno del usuario: dos monitores (DP-1 2560x1440 principal, HDMI-A-1). El juego falló una vez con "Cannot find desired video mode" a pantalla completa; se usa `Windowed=1` en `re3.ini` (copia en `re3.ini.bak`). re3 **reescribe `re3.ini` al salir**: la resolución se cambia desde el menú del juego. El shell es fish; no hay `gh` instalado.

## 5. Limitaciones conocidas

- Las **ruedas de los coches ignoran los voxels**: un coche sobre un bloque se sostiene pero flota/desliza, sin agarre.
- **La IA no esquiva bloques**: peatones y coches intentan pasar y se quedan pegados como ante un muro. Rozar una pared a velocidad hace rotar el coche como un derrape.
- Subir a un bloque de 1 m saltando depende del salto de GTA (a veces no llega).
- El mundo de bloques es **global**, no por partida guardada.
- Sin `unzip` por defecto en Windows: la descarga de texturas falla allí (cae a colores planos). Solo probado en Linux.
- La compilación premake no incluye `src/minecraft` (el modo queda apagado ahí porque el interruptor es de CMake).
- Con textura el clic/rueda dependen de que estén mapeados en los controles de GTA.

## 6. Pendiente, por prioridad sugerida

1. **Salto estilo Minecraft / auto-escalón.** Idea: en `McMode::Update`, cuando el jugador está en modo Steve y choca horizontalmente con un escalón de ≤ 1 bloque, subirlo (auto-step), y/o aumentar el impulso del salto (el salto de GTA está en `Ped.cpp` ~8085-8120). Cuidar que no rompa `SetLanding`/`bIsStanding`.
2. **Modelo/skin de Steve** al activar F8. Investigar cómo se cambia el modelo del jugador en re3 (`CPedModelInfo`, `models/ped`, carpeta `skins/` del juego). La skin de Steve se podría extraer del mismo `client.jar` de Mojang en tiempo de ejecución (como los bloques), sin redistribuirla. Alternativa: modelo cúbico propio.
3. **Ruedas sobre bloques (enfoque B).** Alimentar los sensores de rueda de `CAutomobile::ProcessEntityCollision` (`m_aWheelColPoints`, `m_aSuspensionSpringRatio`) con impactos de `McRay`, tras `#ifdef`. Riesgo: vuelcos/rebotes. Ya existe el análisis en `docs/superpowers/specs/2026-09-30-minecraft-entity-collision-design.md`.
4. **Más bloques y texturas**: añadir ids en `McBlocks`, nombres en `BlockTextureFile`, y crecer el atlas (hoy 4x4 tiles, tile = id; con más de 15 bloques hay que pasar a 8x8). Texturas por cara (césped), bloques con transparencia distinta.
5. **IA que evite bloques** (rutas/`CPathFind` o desvío simple) para peatones y coches.
6. **Mundo por partida**: ligar `mcworld.dat` a la partida guardada (hoy es global).
7. **Pulido de la hotbar**: filtro `nearest` en los iconos (hoy aparecen algo suavizados, aceptado), subtítulos de misión que se solapan con la barra, respetar `CHud::m_Wants_To_Draw_Hud`.
8. **Fase 2: puente con Minecraft real** (estilo SkyCraft: proceso de Minecraft con Fabric + memoria compartida + inyección de render en re3). Solo si se quiere fidelidad total de física/mobs.
9. **Cierre**: revisión final de la rama completa y decidir cómo integrarla (PR hacia `master` o mantenerla como rama). Por ahora **no se ha hecho merge**.
10. **Pequeños pendientes** (de las revisiones, ver sección 7): F8 no se procesa en pausa; ciclo de vida al reiniciar partida (`ShutDownForRestart`); `onGround`/offset de pies 1.0 vs `FEET_OFFSET` 1.04; colocar bloques sobre peatones/vehículos no se rechaza; reproducción de replays empuja vehículos.

## 7. Menores diferidos (para consultar, no urgentes)

Cobertura de tests: caras de borde en `World::Set`; casos u/v del push de `McOrientedBox`; casos negativos del raycast; NaN en la hotbar. Robustez: carrera si se reinicia `McAtlas::Init` mientras un `unzip` antiguo sigue escribiendo; bomba de descompresión PNG (< 1 MB aceptado); `readPNG` pierde unos KB por tile (código de librw); `GetColModel()` no se comprueba en `BuildVehicleBox`; descarga solo desde Linux probada; `MarkAllDirty` redundante con `meshes.clear()`; al llegar el atlas se re-malla todo en un fotograma.

## 8. Cómo retomar el trabajo

Flujo usado (skills de superpowers): brainstorming → spec en `docs/superpowers/specs/` → plan en `docs/superpowers/plans/` → ejecución con subagentes (un implementador por tarea, revisión de cumplimiento y calidad, rondas de corrección, revisión final). Los libros de registro (`.superpowers/sdd/…`) no se versionan; si faltan, basta con este documento, los specs/plans y `git log`. La memoria de Claude de este proyecto está en `~/.claude/projects/-home-akilex-Descargas-gtas/memory/`.

Los commits de la rama llevan el trailer `Co-Authored-By: Claude Sonnet 5.5`.

## 9. Núcleo de supervivencia (MinecraftOSS)

Spec: `docs/superpowers/specs/2026-09-30-minecraft-survival-core-design.md` (incluye la hoja de ruta basada en SkyCraft en su sección 11). Plan: `docs/superpowers/plans/2026-09-30-minecraft-survival-core.md` (contiene la lista de desviaciones respecto al spec).

**Qué hay.** Un crate Rust `mc_bridge` (`src/minecraft/rust/mc_bridge`, staticlib con API C en `src/minecraft/rust/mc_bridge.h`) envuelve `minecraftoss-player` (inventario, crafteo con las recetas reales de Mojang, minería con tiempos y herramientas, desgaste, loot). El mundo de voxels sigue en C++; Rust lo ve por callbacks `get/set` por celda.

**Compilar.** Hace falta `cargo`, `liblzma` y `libbz2` (el crate `zip` los pide: `liblzma-dev`/`libbz2-dev` o equivalentes). Opciones CMake: `RE3_MINECRAFT_SURVIVAL` (ON por defecto) y `RE3_MINECRAFTOSS_DIR` (por defecto `../2010-rust-rewrite-mashup-main/third_party/minecraftoss`, relativo a la raíz del repo: en otra máquina hay que fijarlo). Sin `cargo` o sin ese directorio el modo Steve compila igual, sin supervivencia. CMake crea un symlink `src/minecraft/rust/mc_bridge/minecraftoss` (ignorado por git). Tests de Rust: `cd src/minecraft/rust/mc_bridge && MC_CLIENT_JAR=<ruta al client.jar> MC_ITEM_CATALOG=<ruta al catálogo> cargo test` (sin esas variables los tests con datos reales imprimen `SKIPPED`).

**Datos de Mojang (no se versionan).** `mcassets/client.jar` ahora **persiste** (lo lee el núcleo; la descarga va a `client.jar.part` y se renombra al terminar). Además hace falta `mcassets/item-catalog-26.3.json`: desde la carpeta del juego, `sh /home/akilex/Descargas/gtas/re3/scripts/minecraft/fetch-catalogs.sh` lo descomprime del mashup. Mientras falte alguno de los dos, el modo funciona como antes (hotbar clásica de 4 bloques).

**Controles con supervivencia lista.** F8 activa el modo; **F9** construye junto al jugador un parche de prueba (piedra con menas de carbón y hierro, un árbol, arena y grava); 1-9 y rueda eligen ranura; **clic izquierdo mantenido** mina (barra de progreso bajo la mira); clic derecho coloca el bloque de la ranura (consume 1) y sobre una mesa de crafteo abre la 3x3; **E** abre/cierra el inventario 2x2 (con la pantalla abierta el jugador queda quieto; Esc abre el menú de pausa de GTA, no cierra la pantalla). Inventario en `mcsurvival.dat` (junto a `mcworld.dat`, global).

**Código nuevo.** Rust: `lib.rs` (API C), `survival.rs`, `world.rs` (World sobre callbacks), `catalog.rs` (dureza y herramientas de vanilla generadas), `names.rs`, `coords.rs`. C++ core (con tests): `McItemTable` (57 items, ids 1..29 bloques; 1..4 conservan el valor histórico), `McStarterPatch`, `McInventoryLayout`. C++ game: `McSurvival` (namespace `McSurv`, porque el struct C `McSurvival` ocupa ese nombre), `McInventoryUI`; `McHotbar`, `McInteract`, `McMode` ramifican por `McSurv::IsReady()`. El atlas pasó a 8x8 (tile = id de item).

**Límites conocidos de esta fase.** Sin horno funcional (no se pueden fundir lingotes: el hierro solo se fabrica si se tienen) ni antorcha; sin items en el suelo (los drops van al inventario; si no caben o el item no está en la tabla se pierden con un aviso en el log); recetas con resultados fuera de los 57 items quedan ocultas (escaleras, puertas, cofres...); el inventario solo se autoguarda cuando cambia el mundo (si solo fabricas, se guarda al salir); un pánico a mitad de `mine` puede dejar el bloque roto sin drops (se captura, no cruza a C++). Los textos de cantidad se dibujan en la cola de fuentes de GTA, por encima de los iconos.

**Sin probar en el juego.** Todo compila, enlaza y pasa tests (`mctests` 146, `cargo test` 32), pero ningún cambio de las tareas 2 a 5 se ha visto en ejecución. Primer recorrido manual a hacer: F8, F9, minar el tronco con la mano (lento), fabricar tablones/mesa/palos/pico (E y mesa 3x3), minar piedra con el pico (debe soltar adoquín y desgastar la herramienta), colocar bloques, salir y volver (persistencia), y comprobar que las 4 texturas clásicas se ven como antes.

**Siguientes fases** (hoja de ruta en la sección 11 del spec): movimiento Minecraft con la física de `Player::tick`, combate con daño escalado, cavar el mundo de GTA, IA que esquiva bloques, mundos por partida, generación de terreno.
