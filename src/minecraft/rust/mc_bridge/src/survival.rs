//! Survival state behind the C API: inventory and crafting, block mining and saving. Ids are
//! the C++ item table's; items the table does not know never reach C++.
use crate::catalog::{is_curated_block, mining_catalog_json};
use crate::coords::{to_mc_point, yaw_pitch};
use crate::names::Names;
use crate::world::{CWorld, GetFn, SetFn};
use anyhow::{bail, Context, Result};
use glam::DVec3;
use minecraftoss_player::crafting::RecipeBook;
use minecraftoss_player::inventory::{Inventory, ItemStack};
use minecraftoss_player::item_catalog::ItemCatalog;
use minecraftoss_player::loot::LootBook;
use minecraftoss_player::mining::{Mining, MiningCatalog};
use minecraftoss_player::Player;
use serde_json::{json, Value};
use std::os::raw::c_void;
use std::path::Path;
use std::sync::Arc;

pub const AREA_INVENTORY: i32 = 0;
pub const AREA_GRID_2X2: i32 = 1;
pub const AREA_GRID_3X3: i32 = 2;
pub const AREA_CURSOR: i32 = 3;
pub const AREA_OUTPUT_2X2: i32 = 4;
pub const AREA_OUTPUT_3X3: i32 = 5;

/// Slots C++ sees: 0..9 hotbar, 9..36 backpack, 36..40 armour.
const VISIBLE_SLOTS: usize = 40;
const SAVED_SLOTS: usize = 43;
const HOTBAR: usize = 9;
const EYE_HEIGHT: f64 = 1.62;
const LOOT_SEED: u64 = 0x5eed;
/// Eyes farther than this (or not finite) see nothing: far beyond the GTA map, and it keeps
/// every cell the ray visits well inside `i32`.
const MAX_EYE_COORD: f64 = 1.0e6;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct McStack {
    pub item: i32,
    pub count: i32,
    pub damage: i32,
    pub max_damage: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct McMineResult {
    pub broken: i32,
    pub block_id: i32,
    pub progress: f32,
    /// Items that went into the inventory.
    pub drops_added: i32,
    pub tool_broke: i32,
    /// Items dropped but not kept: unknown to the item table or no room for them.
    pub drops_lost: i32,
}

/// Names for the C++ item table: the leading run of curated blocks after air are its blocks.
pub fn names_from_table(names: Vec<String>) -> Names {
    let last_block = names
        .iter()
        .skip(1)
        .take_while(|name| is_curated_block(name))
        .count() as i32;
    Names::new(names, last_block)
}

pub struct McSurvival {
    pub names: Names,
    pub inventory: Inventory,
    pub mining: Mining,
}

impl McSurvival {
    pub fn new(jar: &Path, catalog_json: &Path, names: Names) -> Result<Self> {
        let catalog = ItemCatalog::from_path(catalog_json)?;
        let book = RecipeBook::from_jar(jar)?.with_item_catalog(Arc::new(catalog));
        let mut mining = Mining::with_catalog(MiningCatalog::from_slice(&mining_catalog_json())?);
        mining.set_loot(LootBook::from_jar(jar)?);
        mining.set_loot_seed(LOOT_SEED);
        Ok(Self {
            names,
            inventory: Inventory::default().with_recipes(book),
            mining,
        })
    }

    /// No recipes, loot or item catalog: for tests that need none of them.
    #[cfg(test)]
    pub(crate) fn bare(names: Names) -> Self {
        Self {
            names,
            inventory: Inventory::default(),
            mining: Mining::with_catalog(
                MiningCatalog::from_slice(&mining_catalog_json()).unwrap(),
            ),
        }
    }

    pub fn stack_to_c(&self, stack: &ItemStack) -> McStack {
        let (damage, max_damage) = self.inventory.recipes.durability(stack).unwrap_or((0, 0));
        McStack {
            item: self.names.id(&stack.id).unwrap_or(0),
            count: stack.count as i32,
            damage: damage as i32,
            max_damage: max_damage as i32,
        }
    }

    pub fn stack_from_c(&self, item: i32, count: i32) -> Option<ItemStack> {
        let name = self.names.name(item)?;
        let count = u8::try_from(count).ok().filter(|&c| c > 0)?;
        Some(self.inventory.recipes.stack(name, count))
    }

    fn known_output(&self, output: Option<ItemStack>) -> Option<ItemStack> {
        output.filter(|stack| self.names.id(&stack.id).is_some())
    }

    fn stack_at(&self, area: i32, index: i32) -> Option<ItemStack> {
        let index = usize::try_from(index).ok()?;
        let inv = &self.inventory;
        match area {
            AREA_INVENTORY if index < VISIBLE_SLOTS => inv.slots[index].clone(),
            AREA_GRID_2X2 => inv.crafting.get(index)?.clone(),
            AREA_GRID_3X3 => inv.workbench.get(index)?.clone(),
            AREA_CURSOR if index == 0 => inv.cursor.clone(),
            AREA_OUTPUT_2X2 if index == 0 => self.known_output(inv.crafting_output()),
            AREA_OUTPUT_3X3 if index == 0 => self.known_output(inv.workbench_output()),
            _ => None,
        }
    }

    /// The stack shown at `area`/`index`; `None` when empty, invalid or unknown to C++.
    pub fn get(&self, area: i32, index: i32) -> Option<McStack> {
        let stack = self.stack_at(area, index)?;
        Some(self.stack_to_c(&stack)).filter(|c| c.item != 0)
    }

    /// A click on an inventory or grid slot (outputs go through `take_output`).
    pub fn click(&mut self, area: i32, index: i32, right: bool, shift: bool) -> bool {
        let Ok(index) = usize::try_from(index) else {
            return false;
        };
        match area {
            AREA_INVENTORY if index < VISIBLE_SLOTS => {
                // Only a click outside the menu drops a stack, and this is a slot click.
                let _ = self.inventory.click(Some(index), right, shift);
            }
            AREA_GRID_2X2 if index < 4 => self.inventory.click_crafting_slot(index, right, shift),
            AREA_GRID_3X3 if index < 9 => self.inventory.click_workbench_slot(index, right, shift),
            _ => return false,
        }
        true
    }

    /// Crafts once (or as often as fits with `shift`); refuses outputs unknown to C++.
    pub fn take_output(&mut self, workbench: bool, shift: bool) -> bool {
        let output = if workbench {
            self.inventory.workbench_output()
        } else {
            self.inventory.crafting_output()
        };
        if self.known_output(output).is_none() {
            return false;
        }
        if workbench {
            self.inventory.take_workbench_output(shift)
        } else {
            self.inventory.take_crafting_output(shift)
        }
    }

    /// Returns both grids and the cursor to the inventory; the number of stacks that did not fit.
    pub fn close(&mut self) -> i32 {
        let mut lost = self.inventory.settle_crafting().len();
        lost += self.inventory.settle_workbench().len();
        lost += usize::from(self.inventory.settle_cursor().is_some());
        lost as i32
    }

    /// Adds `count` of `item`; the leftover that did not fit, or `None` for bad input.
    pub fn add(&mut self, item: i32, count: i32) -> Option<i32> {
        let stack = self.stack_from_c(item, count)?;
        Some(
            self.inventory
                .add_item(stack, 0)
                .map_or(0, |left| left.count as i32),
        )
    }

    /// Removes `count` items from a visible slot holding at least that many.
    pub fn consume(&mut self, slot: i32, count: i32) -> bool {
        let Ok(slot) = usize::try_from(slot) else {
            return false;
        };
        let Ok(count) = u8::try_from(count) else {
            return false;
        };
        if slot >= VISIBLE_SLOTS || count == 0 {
            return false;
        }
        let entry = &mut self.inventory.slots[slot];
        let Some(stack) = entry.as_mut().filter(|stack| stack.count >= count) else {
            return false;
        };
        stack.count -= count;
        if stack.count == 0 {
            *entry = None;
        }
        true
    }

    /// One 1/20 s mining tick from a GTA eye position and look direction, with the hotbar
    /// slot `selected` in hand. `None` when `selected` is not a hotbar slot.
    ///
    /// # Safety
    /// `get` and `set` must be safe to call with `ctx` during this call.
    #[allow(clippy::too_many_arguments)]
    pub unsafe fn mine(
        &mut self,
        get: GetFn,
        set: SetFn,
        ctx: *mut c_void,
        eye: [f64; 3],
        dir: [f64; 3],
        attacking: bool,
        on_ground: bool,
        selected: i32,
    ) -> Option<McMineResult> {
        let selected = usize::try_from(selected).ok().filter(|&s| s < HOTBAR)?;
        if !eye.iter().all(|v| v.abs() <= MAX_EYE_COORD) {
            self.mining.reset();
            return Some(McMineResult::default());
        }
        let mut player = Player::new(to_mc_point(eye) - DVec3::new(0.0, EYE_HEIGHT, 0.0));
        (player.yaw, player.pitch) = yaw_pitch(dir);
        player.on_ground = on_ground;
        let mut world = CWorld::new(get, set, ctx, &self.names);
        let held = self.inventory.slots[selected].clone();
        let broken = self
            .mining
            .tick(&mut world, &player, held.as_ref(), attacking);
        let mut result = McMineResult {
            progress: self.mining.progress(),
            ..McMineResult::default()
        };
        let Some(broken) = broken else {
            return Some(result);
        };
        result.broken = 1;
        result.block_id = self.names.id(&broken.block.id).unwrap_or(0);
        for drop in broken.drops {
            let count = drop.count as i32;
            if self.names.id(&drop.id).is_none() {
                result.drops_lost += count;
                continue;
            }
            let left = self
                .inventory
                .add_item(drop, selected)
                .map_or(0, |left| left.count as i32);
            result.drops_added += count - left;
            result.drops_lost += left;
        }
        result.tool_broke = self
            .inventory
            .wear_tool_after_mining(selected, broken.hardness) as i32;
        Some(result)
    }

    /// Writes every inventory slot as JSON, through a temporary file and a rename.
    pub fn save(&self, path: &Path) -> bool {
        let slots: Vec<Value> = self
            .inventory
            .slots
            .iter()
            .map(|slot| match slot {
                None => Value::Null,
                Some(stack) => json!({
                    "id": stack.id,
                    "count": stack.count,
                    "components": stack.components,
                }),
            })
            .collect();
        let mut tmp = path.as_os_str().to_owned();
        tmp.push(".tmp");
        let bytes = json!({ "slots": slots }).to_string();
        std::fs::write(&tmp, bytes).is_ok() && std::fs::rename(&tmp, path).is_ok()
    }

    /// Replaces the inventory with a saved one. A missing or corrupt file leaves it empty.
    pub fn load(&mut self, path: &Path) -> bool {
        let mut inventory = Inventory::default();
        inventory.recipes = Arc::clone(&self.inventory.recipes);
        let loaded = self.read_slots(path);
        let ok = loaded.is_ok();
        if let Ok(slots) = loaded {
            inventory.slots = slots;
        }
        self.inventory = inventory;
        ok
    }

    fn read_slots(&self, path: &Path) -> Result<Vec<Option<ItemStack>>> {
        let bytes = std::fs::read(path).with_context(|| format!("reading {}", path.display()))?;
        let root: Value = serde_json::from_slice(&bytes)?;
        let Some(raw) = root["slots"].as_array().filter(|s| s.len() == SAVED_SLOTS) else {
            bail!("expected {SAVED_SLOTS} slots");
        };
        raw.iter()
            .map(|slot| {
                if slot.is_null() {
                    return Ok(None);
                }
                let id = slot["id"].as_str().context("slot without id")?;
                let count = slot["count"]
                    .as_u64()
                    .and_then(|c| u8::try_from(c).ok())
                    .filter(|&c| c > 0)
                    .context("bad slot count")?;
                let mut stack = self.inventory.recipes.stack(id, count);
                stack.components = Some(slot["components"].clone()).filter(|c| !c.is_null());
                Ok(Some(stack))
            })
            .collect()
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;
    use crate::world::{GetFn, SetFn};
    use std::collections::HashMap;
    use std::io::Write;
    use std::os::raw::c_void;
    use std::path::PathBuf;

    /// The real 26.3 client jar and item catalog, or a visible SKIPPED line on stderr (written
    /// past the test harness capture) when the environment does not name them.
    pub(crate) fn real_data(test: &str) -> Option<(PathBuf, PathBuf)> {
        match (
            std::env::var_os("MC_CLIENT_JAR"),
            std::env::var_os("MC_ITEM_CATALOG"),
        ) {
            (Some(jar), Some(catalog)) => Some((jar.into(), catalog.into())),
            _ => {
                let _ = writeln!(
                    std::io::stderr(),
                    "SKIPPED {test}: set MC_CLIENT_JAR/MC_ITEM_CATALOG to run it"
                );
                None
            }
        }
    }

    pub(crate) fn names(list: &[&str], last_block: i32) -> Names {
        Names::new(list.iter().map(|n| n.to_string()).collect(), last_block)
    }

    // Blocks 1..=6, then items.
    const TEST_NAMES: &[&str] = &[
        "minecraft:air",
        "minecraft:dirt",
        "minecraft:stone",
        "minecraft:cobblestone",
        "minecraft:oak_log",
        "minecraft:oak_planks",
        "minecraft:crafting_table",
        "minecraft:stick",
        "minecraft:wooden_pickaxe",
    ];
    const DIRT: i32 = 1;
    const STONE: i32 = 2;
    const COBBLESTONE: i32 = 3;
    const OAK_LOG: i32 = 4;
    const OAK_PLANKS: i32 = 5;
    const CRAFTING_TABLE: i32 = 6;
    const STICK: i32 = 7;
    const WOODEN_PICKAXE: i32 = 8;

    fn real_survival(test: &str, table: Names) -> Option<McSurvival> {
        let (jar, catalog) = real_data(test)?;
        Some(McSurvival::new(&jar, &catalog, table).expect("loading the real 26.3 data"))
    }

    /// The C++ grid: GTA cells to ids, through the same C callbacks the game passes.
    #[derive(Default)]
    struct GtaGrid(HashMap<[i32; 3], i32>);

    unsafe extern "C" fn grid_get(ctx: *mut c_void, x: i32, y: i32, z: i32) -> i32 {
        let grid = &*(ctx as *const GtaGrid);
        grid.0.get(&[x, y, z]).copied().unwrap_or(0)
    }
    unsafe extern "C" fn grid_set(ctx: *mut c_void, x: i32, y: i32, z: i32, id: i32) {
        let grid = &mut *(ctx as *mut GtaGrid);
        if id == 0 {
            grid.0.remove(&[x, y, z]);
        } else {
            grid.0.insert([x, y, z], id);
        }
    }

    // Eye in GTA cell (0,0,1), looking north at the cell two steps ahead.
    const EYE: [f64; 3] = [0.5, 0.5, 1.5];
    const NORTH: [f64; 3] = [0.0, 1.0, 0.0];
    const TARGET: [i32; 3] = [0, 2, 1];

    fn grid_with(id: i32) -> GtaGrid {
        let mut grid = GtaGrid::default();
        grid.0.insert(TARGET, id);
        grid
    }

    fn tick(s: &mut McSurvival, grid: &mut GtaGrid, selected: i32) -> McMineResult {
        let get: GetFn = grid_get;
        let set: SetFn = grid_set;
        let ctx = grid as *mut GtaGrid as *mut c_void;
        // SAFETY: the callbacks only touch `grid`, which outlives the call.
        unsafe { s.mine(get, set, ctx, EYE, NORTH, true, true, selected) }.unwrap()
    }

    /// Ticks until the block breaks; returns the tick number and its result.
    fn mine_until_broken(
        s: &mut McSurvival,
        grid: &mut GtaGrid,
        selected: i32,
    ) -> (u32, McMineResult) {
        for n in 1..=1000 {
            let r = tick(s, grid, selected);
            if r.broken != 0 {
                return (n, r);
            }
        }
        panic!("block never broke");
    }

    fn slot(s: &McSurvival, index: i32) -> Option<McStack> {
        s.get(AREA_INVENTORY, index)
    }

    #[test]
    fn mining_dirt_by_hand_takes_the_vanilla_time() {
        let Some(mut s) = real_survival("mining_dirt", names(TEST_NAMES, 6)) else {
            return;
        };
        let mut grid = grid_with(DIRT);
        for _ in 0..14 {
            assert_eq!(tick(&mut s, &mut grid, 0).broken, 0);
        }
        assert!((s.mining.progress() - 14.0 / 15.0).abs() < 1e-5);
        let r = tick(&mut s, &mut grid, 0);
        assert_eq!(r.broken, 1);
        assert_eq!(r.block_id, DIRT);
        assert_eq!(r.drops_added, 1);
        assert_eq!(r.drops_lost, 0);
        assert_eq!(r.tool_broke, 0);
        assert!(grid.0.is_empty(), "the broken cell is cleared through set");
        let got = slot(&s, 0).unwrap();
        assert_eq!((got.item, got.count), (DIRT, 1));
    }

    #[test]
    fn wooden_pickaxe_mines_stone_faster_than_hand() {
        let Some(mut s) = real_survival("wooden_pickaxe", names(TEST_NAMES, 6)) else {
            return;
        };
        let mut grid = grid_with(STONE);
        let (hand_ticks, r) = mine_until_broken(&mut s, &mut grid, 0);
        assert_eq!(hand_ticks, 150);
        assert_eq!((r.drops_added, r.drops_lost), (0, 0));
        assert!(slot(&s, 0).is_none());

        assert_eq!(s.add(WOODEN_PICKAXE, 1), Some(0));
        let mut grid = grid_with(STONE);
        let (pick_ticks, r) = mine_until_broken(&mut s, &mut grid, 0);
        assert_eq!(pick_ticks, 23);
        assert_eq!(r.block_id, STONE);
        assert_eq!(r.drops_added, 1);
        let drop = slot(&s, 1).unwrap();
        assert_eq!((drop.item, drop.count), (COBBLESTONE, 1));
        let pick = slot(&s, 0).unwrap();
        assert_eq!(
            (pick.item, pick.damage, pick.max_damage),
            (WOODEN_PICKAXE, 1, 59)
        );
    }

    #[test]
    fn unknown_or_air_cell_does_not_break() {
        let mut s = McSurvival::bare(names(TEST_NAMES, 6));
        for id in [0, 200, STICK, -5] {
            let mut grid = grid_with(id);
            for _ in 0..200 {
                let r = tick(&mut s, &mut grid, 0);
                assert_eq!(r.broken, 0);
                assert_eq!(r.progress, 0.0);
            }
        }
    }

    #[test]
    fn mining_with_a_bad_slot_is_refused() {
        let mut s = McSurvival::bare(names(TEST_NAMES, 6));
        let mut grid = grid_with(DIRT);
        let ctx = &mut grid as *mut GtaGrid as *mut c_void;
        for selected in [-1, 9, 40] {
            // SAFETY: the callbacks only touch `grid`.
            let r = unsafe { s.mine(grid_get, grid_set, ctx, EYE, NORTH, true, true, selected) };
            assert!(r.is_none());
        }
    }

    #[test]
    fn unknown_drop_is_discarded_and_counted() {
        // grass_block drops dirt, which this table does not know.
        let table = names(&["minecraft:air", "minecraft:grass_block"], 1);
        let Some(mut s) = real_survival("unknown_drop", table) else {
            return;
        };
        let mut grid = grid_with(1);
        let (_, r) = mine_until_broken(&mut s, &mut grid, 0);
        assert_eq!((r.block_id, r.drops_added, r.drops_lost), (1, 0, 1));
        assert!(s.inventory.slots.iter().all(Option::is_none));
    }

    #[test]
    fn tool_wears_and_breaks() {
        let Some(mut s) = real_survival("tool_wears", names(TEST_NAMES, 6)) else {
            return;
        };
        let mut pick = s.stack_from_c(WOODEN_PICKAXE, 1).unwrap();
        pick.components = Some(serde_json::json!({ "minecraft:damage": 58 }));
        s.inventory.slots[0] = Some(pick);
        let before = slot(&s, 0).unwrap();
        assert_eq!((before.damage, before.max_damage), (58, 59));
        let mut grid = grid_with(DIRT);
        let (_, r) = mine_until_broken(&mut s, &mut grid, 0);
        assert_eq!(r.tool_broke, 1);
        assert!(slot(&s, 0).is_none());
        assert_eq!(slot(&s, 1).map(|d| d.item), Some(DIRT));
    }

    #[test]
    fn full_inventory_returns_leftover() {
        let mut s = McSurvival::bare(names(TEST_NAMES, 6));
        for _ in 0..36 {
            assert_eq!(s.add(DIRT, 64), Some(0));
        }
        assert_eq!(s.add(DIRT, 10), Some(10));
        assert_eq!(s.add(STONE, 3), Some(3));
    }

    #[test]
    fn add_and_consume_validate_their_input() {
        let mut s = McSurvival::bare(names(TEST_NAMES, 6));
        assert_eq!(s.add(0, 1), None);
        assert_eq!(s.add(99, 1), None);
        assert_eq!(s.add(DIRT, 0), None);
        assert_eq!(s.add(DIRT, 256), None);
        assert_eq!(s.add(DIRT, 5), Some(0));
        assert!(!s.consume(0, 6));
        assert!(!s.consume(-1, 1));
        assert!(!s.consume(40, 1));
        assert!(!s.consume(0, 0));
        assert!(s.consume(0, 2));
        assert_eq!(slot(&s, 0).map(|d| d.count), Some(3));
        assert!(s.consume(0, 3));
        assert!(slot(&s, 0).is_none());
    }

    #[test]
    fn get_and_click_reject_bad_areas_and_indices() {
        let mut s = McSurvival::bare(names(TEST_NAMES, 6));
        s.add(DIRT, 1);
        for (area, index) in [
            (0, -1),
            (0, 40),
            (1, 4),
            (2, 9),
            (3, 1),
            (4, 1),
            (5, 1),
            (6, 0),
        ] {
            assert!(s.get(area, index).is_none(), "get {area}/{index}");
            assert!(!s.click(area, index, false, false), "click {area}/{index}");
        }
        assert!(!s.click(AREA_CURSOR, 0, false, false));
        assert!(!s.click(AREA_OUTPUT_2X2, 0, false, false));
        assert!(s.click(AREA_INVENTORY, 0, false, false));
        assert_eq!(s.get(AREA_CURSOR, 0).map(|c| c.item), Some(DIRT));
    }

    fn find(s: &McSurvival, item: i32) -> i32 {
        (0..36)
            .find(|&i| slot(s, i).is_some_and(|st| st.item == item))
            .expect("item in inventory")
    }

    #[test]
    fn crafting_logs_to_planks_to_table_to_pickaxe() {
        let Some(mut s) = real_survival("crafting", names(TEST_NAMES, 6)) else {
            return;
        };
        // log -> 4 planks
        s.add(OAK_LOG, 1);
        assert!(s.click(AREA_INVENTORY, 0, false, false));
        assert!(s.click(AREA_GRID_2X2, 0, false, false));
        let out = s.get(AREA_OUTPUT_2X2, 0).unwrap();
        assert_eq!((out.item, out.count), (OAK_PLANKS, 4));
        assert!(s.take_output(false, false));
        assert!(s.get(AREA_GRID_2X2, 0).is_none());
        let cursor = s.get(AREA_CURSOR, 0).unwrap();
        assert_eq!((cursor.item, cursor.count), (OAK_PLANKS, 4));

        // 4 planks in the 2x2 -> crafting table
        for i in 0..4 {
            assert!(s.click(AREA_GRID_2X2, i, true, false));
        }
        assert!(s.get(AREA_CURSOR, 0).is_none());
        let out = s.get(AREA_OUTPUT_2X2, 0).unwrap();
        assert_eq!((out.item, out.count), (CRAFTING_TABLE, 1));
        assert!(s.take_output(false, true));
        assert_eq!(slot(&s, find(&s, CRAFTING_TABLE)).unwrap().count, 1);

        // 2 planks stacked vertically -> 4 sticks
        s.add(OAK_PLANKS, 5);
        assert!(s.click(AREA_INVENTORY, find(&s, OAK_PLANKS), false, false));
        assert!(s.click(AREA_GRID_2X2, 0, true, false));
        assert!(s.click(AREA_GRID_2X2, 2, true, false));
        let out = s.get(AREA_OUTPUT_2X2, 0).unwrap();
        assert_eq!((out.item, out.count), (STICK, 4));
        assert!(s.take_output(false, true));
        assert_eq!(slot(&s, find(&s, STICK)).unwrap().count, 4);

        // 3x3: planks on top, sticks down the middle -> wooden pickaxe
        for i in 0..3 {
            assert!(s.click(AREA_GRID_3X3, i, true, false));
        }
        assert!(s.get(AREA_CURSOR, 0).is_none());
        let sticks = find(&s, STICK);
        assert!(s.click(AREA_INVENTORY, sticks, false, false));
        assert!(s.click(AREA_GRID_3X3, 4, true, false));
        assert!(s.click(AREA_GRID_3X3, 7, true, false));
        assert!(s.click(AREA_INVENTORY, sticks, false, false));
        assert!(s.get(AREA_CURSOR, 0).is_none());
        assert!(s.get(AREA_OUTPUT_2X2, 0).is_none());
        let out = s.get(AREA_OUTPUT_3X3, 0).unwrap();
        assert_eq!(
            (out.item, out.count, out.max_damage),
            (WOODEN_PICKAXE, 1, 59)
        );
        assert!(s.take_output(true, false));
        assert_eq!(s.get(AREA_CURSOR, 0).unwrap().item, WOODEN_PICKAXE);
        assert!(s.get(AREA_OUTPUT_3X3, 0).is_none());
    }

    #[test]
    fn unknown_crafting_output_is_hidden() {
        // No crafting_table in this table: 4 planks craft something C++ cannot show.
        let table = names(&["minecraft:air", "minecraft:oak_planks"], 1);
        let Some(mut s) = real_survival("unknown_output", table) else {
            return;
        };
        s.add(1, 4);
        assert!(s.click(AREA_INVENTORY, 0, false, false));
        for i in 0..4 {
            assert!(s.click(AREA_GRID_2X2, i, true, false));
        }
        assert!(s.inventory.crafting_output().is_some());
        assert!(s.get(AREA_OUTPUT_2X2, 0).is_none());
        assert!(!s.take_output(false, false));
        assert!(!s.take_output(false, true));
        assert!((0..4).all(|i| s.get(AREA_GRID_2X2, i).is_some()));
    }

    /// A temporary directory removed when dropped.
    pub(crate) struct ScratchDir(PathBuf);
    impl ScratchDir {
        pub(crate) fn new(test: &str) -> Self {
            let dir = std::env::temp_dir().join(format!("mc_bridge_{test}_{}", std::process::id()));
            std::fs::create_dir_all(&dir).unwrap();
            Self(dir)
        }
        pub(crate) fn file(&self, name: &str) -> PathBuf {
            self.0.join(name)
        }
    }
    impl Drop for ScratchDir {
        fn drop(&mut self) {
            let _ = std::fs::remove_dir_all(&self.0);
        }
    }

    #[test]
    fn save_load_round_trip() {
        let Some(mut s) = real_survival("save_load", names(TEST_NAMES, 6)) else {
            return;
        };
        let mut pick = s.stack_from_c(WOODEN_PICKAXE, 1).unwrap();
        pick.components = Some(serde_json::json!({ "minecraft:damage": 7 }));
        s.inventory.slots[3] = Some(pick);
        s.add(DIRT, 20);
        s.inventory.slots[36] = s.stack_from_c(STONE, 1);
        let dir = ScratchDir::new("save_load");
        let path = dir.file("round_trip.json");
        assert!(s.save(&path));
        let saved: Vec<_> = (0..40).map(|i| slot(&s, i)).collect();

        let Some(mut t) = real_survival("save_load", names(TEST_NAMES, 6)) else {
            return;
        };
        assert!(t.load(&path));
        let loaded: Vec<_> = (0..40).map(|i| slot(&t, i)).collect();
        assert_eq!(loaded, saved);
        assert_eq!(t.inventory.slots, s.inventory.slots);
        assert_eq!(slot(&t, 3).unwrap().damage, 7);
        assert!(!path.with_extension("json.tmp").exists());
    }

    #[test]
    fn load_missing_or_corrupt_starts_empty() {
        let mut s = McSurvival::bare(names(TEST_NAMES, 6));
        let dir = ScratchDir::new("load_corrupt");
        let missing = dir.file("does_not_exist.json");
        let corrupt = dir.file("corrupt.json");
        std::fs::write(&corrupt, b"{ not json").unwrap();
        let short = dir.file("short.json");
        std::fs::write(&short, br#"{"slots":[null,null]}"#).unwrap();
        let bad_count = dir.file("bad_count.json");
        let mut slots = vec![serde_json::Value::Null; 43];
        slots[0] = serde_json::json!({ "id": "minecraft:dirt", "count": 0 });
        std::fs::write(
            &bad_count,
            serde_json::json!({ "slots": slots }).to_string(),
        )
        .unwrap();
        for path in [missing, corrupt, short, bad_count] {
            s.add(DIRT, 3);
            assert!(!s.load(&path), "{}", path.display());
            assert!(s.inventory.slots.iter().all(Option::is_none));
        }
    }

    #[test]
    fn inventory_close_returns_grid_and_cursor() {
        let mut s = McSurvival::bare(names(TEST_NAMES, 6));
        s.add(DIRT, 3);
        s.add(STONE, 2);
        assert!(s.click(AREA_INVENTORY, 0, false, false));
        assert!(s.click(AREA_GRID_2X2, 1, true, false));
        assert!(s.click(AREA_GRID_3X3, 8, true, false));
        assert!(s.click(AREA_INVENTORY, 1, false, false));
        assert_eq!(s.close(), 0);
        assert!(s.get(AREA_CURSOR, 0).is_none());
        assert!((0..4).all(|i| s.get(AREA_GRID_2X2, i).is_none()));
        assert!((0..9).all(|i| s.get(AREA_GRID_3X3, i).is_none()));
        assert_eq!(s.inventory.count("minecraft:dirt"), 3);
        assert_eq!(s.inventory.count("minecraft:stone"), 2);
    }

    #[test]
    fn inventory_close_counts_what_does_not_fit() {
        let mut s = McSurvival::bare(names(TEST_NAMES, 6));
        s.inventory.cursor = s.stack_from_c(STONE, 1);
        s.inventory.crafting[0] = s.stack_from_c(STICK, 1);
        for _ in 0..36 {
            s.add(DIRT, 64);
        }
        assert_eq!(s.close(), 2);
        assert!(s.inventory.cursor.is_none());
    }

    #[test]
    fn full_item_table_marks_its_blocks() {
        let mut list = vec!["minecraft:air".to_string()];
        list.extend(
            ["dirt", "stone", "obsidian", "stick", "coal"].map(|n| format!("minecraft:{n}")),
        );
        let table = names_from_table(list);
        assert!(table.is_block(3));
        assert!(!table.is_block(4));
    }
}
