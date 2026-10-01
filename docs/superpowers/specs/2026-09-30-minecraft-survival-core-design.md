# Núcleo de supervivencia con minecraftoss: diseño

Fecha: 2026-09-30. Rama: `minecraft-mode`. Estado: pendiente de revisión.

## 1. Objetivo

Un bucle de supervivencia jugable dentro de GTA III: romper bloques con tiempo y herramienta correctos, recoger los items, abrir un inventario con crafteo 2x2 y una mesa 3x3, y fabricar herramientas que se desgastan. Las reglas (recetas, dureza, drops, desgaste) vienen de MinecraftOSS, un motor en Rust que replica Minecraft Java 26.3, enlazado dentro de re3 como biblioteca estática (un solo proceso).

Fuera de esta fase: generación de terreno, mobs, redstone, fluidos, hambre y vida, horno, mundo por partida guardada. Son fases posteriores y la API Rust ya las soporta.

Criterios de éxito:
- Se rompe un tronco con la mano, se obtienen los items, se fabrican tablones, palos, mesa de crafteo y un pico de madera, y con él se rompe piedra más rápido que con la mano.
- La herramienta pierde durabilidad y se rompe al agotarla.
- El inventario 2x2 y la mesa 3x3 admiten clic izquierdo, derecho, shift y arrastre con el comportamiento de Minecraft, porque lo ejecuta `Inventory::click`.
- Sin `cargo` o sin el `client.jar`, el modo Steve actual sigue funcionando sin supervivencia.

## 2. Arquitectura

```
re3 (C++)                               Rust (staticlib, extern "C")
McWorld / McMesher / McRenderer         mc_bridge --> minecraftoss-player
McInteract (rayo, clic)  --eventos-->     Inventory, Mining, RecipeBook,
McHotbar / McInventoryUI <--estado--      ItemCatalog, LootBook
McItemTable (nombre <-> id <-> tile)
```

- El mundo de voxels sigue en C++. Rust solo conoce al jugador, sus items y las reglas.
- Un objeto Rust opaco `McSurvival` guarda `Inventory`, `Mining` y los catálogos. C++ guarda un puntero y le manda eventos (clic en ranura, tick de minería, abrir mesa) y le pide estado (ranuras, progreso, drops).
- Los nombres de texto de minecraftoss (`minecraft:oak_planks`) cruzan la frontera. `McItemTable` los traduce a ids numéricos y tiles de atlas.
- `Mining::tick` exige un `impl World` y un `Player`. El puente implementa `World` con callbacks a `McWorld` (consulta de bloque por posición) y construye el `Player` con posición y mirada que da C++.
- Ganchos en código de Rockstar: los mismos de una línea tras `#ifdef MINECRAFT_MODE`.

## 3. Construcción

- Se copian `core` y `player` de `2010-rust-rewrite-mashup-main/third_party/minecraftoss` a `src/minecraft/rust/minecraftoss/` (commit de origen `4013a68`, con su licencia y `NOTICE`). `generator`, `world` y `entities` se traen en fases posteriores. No se toca `vendor/`.
- `src/minecraft/rust/mc_bridge/`: `crate-type = ["staticlib"]`, API C con `mc_bridge.h` escrito a mano, `catch_unwind` en cada función exportada para que un pánico no cruce a C++.
- CMake, dentro de `RE3_MINECRAFT_MODE`: nueva opción `RE3_MINECRAFT_SURVIVAL` (ON si se encuentra `cargo`). Un `add_custom_target` ejecuta `cargo build --release` y se enlaza la `.a` con `dl`, `pthread` y `m`. Sin la opción, o sin `cargo`, no se compila nada de Rust ni se define `MINECRAFT_SURVIVAL`.
- Se verifica al compilar la edición de `core` (2024) frente al toolchain instalado (Rust 1.98).

## 4. Datos de Mojang

- Recetas, loot y catálogos se leen del `client.jar` que ya descarga `McAtlas` junto a `mcassets/`. Nada de Mojang se versiona.
- Si el jar o los catálogos faltan, `McSurvival` no se crea y el modo se queda como hoy.
- Riesgo abierto: `MiningCatalog` espera un JSON exportado por un arnés de Fabric (esquema 1, 26.3). El mashup no lo incluye entre sus catálogos comprimidos. El primer paso del plan es comprobar de dónde sale; si no se puede obtener, la minería usa el fallback `BlockInfo::for_id` que ya existe en `mining.rs`, o se genera una tabla de dureza propia para los ~60 ids.

## 5. Catálogo de la fase 1

~60 bloques e items curados, con el ciclo de madera a hierro: tierra, hierba, piedra, adoquín, arena, grava, troncos, tablones, hojas, cristal, mena de carbón y de hierro, lingote, mesa de crafteo, horno (solo bloque), antorcha, palos, carbón, y herramientas de madera, piedra y hierro (pico, hacha, pala, azada, espada).

- `McItemTable`: tabla `nombre <-> id numérico <-> tile`, con tiles propios para items que no son bloques.
- Prueba: cada nombre de la tabla existe en las recetas y catálogos de 26.3.
- El atlas pasa de 4x4 a 16x16 tiles (el id ya no es el índice de tile). Ampliar el catálogo después es añadir una fila.

## 6. Jugabilidad e interfaz

- **Minería**: clic izquierdo mantenido, un `Mining::tick` por cada 1/20 s. Progreso desde Rust; grietas del bloque en 10 etapas. Al romper, los `drops` se recogen como items y la herramienta se desgasta.
- **Inventario**: tecla E abre la pantalla con las 4 ranuras de crafteo, la armadura y las 36 ranuras. Clic derecho sobre una mesa de crafteo abre la 3x3. Todo el comportamiento de clic, shift y arrastre lo ejecuta `Inventory::click` y funciones afines; C++ solo dibuja y traduce el ratón a índices de ranura.
- **Dibujo**: sprites 2D desde el atlas, barra de durabilidad desde `RecipeBook::durability`. La hotbar actual pasa a leer las ranuras 0..9 del inventario.
- **Colocar bloques**: consume un item de la mano.

## 7. Persistencia

El inventario se guarda en un archivo junto a `mcworld.dat` (autoguardado y al salir), con el mismo guardado atómico. Sigue siendo global, no por partida.

## 8. Errores y límites

- Cada función de la API devuelve un código de error. Un valor no válido o un pánico capturado deja a `McSurvival` inutilizable y desactiva la supervivencia sin colgar el juego.
- Los ids desconocidos del lado C++ se tratan como aire, como `GetBlockInfo`.

## 9. Pruebas

- `cargo test` en `mc_bridge`: clic y crafteo, desgaste de herramienta, tick de minería sobre un mundo de prueba.
- `mctests`: `McItemTable`, y la lógica pura de la interfaz (índice de ranura bajo el ratón, como `McHotbarLayout`).
- Prueba manual en el juego, siguiendo el criterio de éxito de la sección 1.

## 10. Orden de implementación sugerido

1. Compilación Rust + CMake y una llamada de humo (`mc_bridge_version`).
2. `McItemTable` y atlas 16x16.
3. Inventario sin interfaz: hotbar leyendo de Rust, colocar consume items.
4. Minería con drops, recogida y desgaste.
5. Pantallas de inventario 2x2 y mesa 3x3.
6. Persistencia y cierre.
