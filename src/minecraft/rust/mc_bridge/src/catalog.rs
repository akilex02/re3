//! Mining catalog (hardness, tool tier, tool speed) for the curated blocks, in the JSON schema
//! `MiningCatalog::from_slice` reads. Values are vanilla 26.3. Every float is written as
//! `{"decimal", "bits"}` because the reader (`exact_f32`) checks both agree.
use serde_json::{json, Map, Value};

struct Row {
    name: &'static str,
    hardness: f32,
    tool: &'static str,
    min_tier: u8,
    requires_tool: bool,
}

const TIERS: [(&str, u8, f32); 4] = [
    ("wooden", 0, 2.0),
    ("stone", 1, 4.0),
    ("iron", 2, 6.0),
    ("diamond", 3, 8.0),
];
const KINDS: [&str; 5] = ["pickaxe", "axe", "shovel", "hoe", "sword"];

const fn row(
    name: &'static str,
    hardness: f32,
    tool: &'static str,
    min_tier: u8,
    requires_tool: bool,
) -> Row {
    Row {
        name,
        hardness,
        tool,
        min_tier,
        requires_tool,
    }
}

const ROWS: &[Row] = &[
    row("dirt", 0.5, "shovel", 0, false),
    row("stone", 1.5, "pickaxe", 0, true),
    row("oak_planks", 2.0, "axe", 0, false),
    row("glass", 0.3, "none", 0, false),
    row("grass_block", 0.6, "shovel", 0, false),
    row("cobblestone", 2.0, "pickaxe", 0, true),
    row("sand", 0.5, "shovel", 0, false),
    row("gravel", 0.6, "shovel", 0, false),
    row("oak_log", 2.0, "axe", 0, false),
    row("oak_leaves", 0.2, "hoe", 0, false),
    row("coal_ore", 3.0, "pickaxe", 0, true),
    row("iron_ore", 3.0, "pickaxe", 1, true),
    row("crafting_table", 2.5, "axe", 0, false),
    row("furnace", 3.5, "pickaxe", 0, true),
    row("birch_log", 2.0, "axe", 0, false),
    row("birch_planks", 2.0, "axe", 0, false),
    row("spruce_log", 2.0, "axe", 0, false),
    row("spruce_planks", 2.0, "axe", 0, false),
    row("bricks", 2.0, "pickaxe", 0, true),
    row("sandstone", 0.8, "pickaxe", 0, true),
    row("stone_bricks", 1.5, "pickaxe", 0, true),
    row("coal_block", 5.0, "pickaxe", 0, true),
    row("iron_block", 5.0, "pickaxe", 1, true),
    row("diamond_ore", 3.0, "pickaxe", 2, true),
    row("diamond_block", 5.0, "pickaxe", 2, true),
    row("gold_ore", 3.0, "pickaxe", 2, true),
    row("gold_block", 3.0, "pickaxe", 2, true),
    row("white_wool", 0.8, "none", 0, false),
    row("obsidian", 50.0, "pickaxe", 3, true),
];

fn exact(value: f32) -> Value {
    json!({ "decimal": format!("{value:?}"), "bits": format!("{:08x}", value.to_bits()) })
}

/// True for the blocks this catalog knows (`minecraft:` ids), i.e. the C++ placeable blocks.
pub fn is_curated_block(name: &str) -> bool {
    name.strip_prefix("minecraft:")
        .is_some_and(|short| ROWS.iter().any(|row| row.name == short))
}

pub fn mining_catalog_json() -> Vec<u8> {
    let mut blocks = Map::new();
    for row in ROWS {
        let mut tools = Map::new();
        for (tier, level, speed) in TIERS {
            for kind in KINDS {
                let matches = kind == row.tool;
                tools.insert(
                    format!("minecraft:{tier}_{kind}"),
                    json!({
                        "speed": exact(if matches { speed } else { 1.0 }),
                        "correct": matches && level >= row.min_tier,
                    }),
                );
            }
        }
        blocks.insert(
            format!("minecraft:{}", row.name),
            json!({
                "hardness": exact(row.hardness),
                "requires_tool": row.requires_tool,
                "tools": Value::Object(tools),
            }),
        );
    }
    let root = json!({
        "schema_version": 1,
        "minecraft_version": "26.3",
        "blocks": Value::Object(blocks),
    });
    serde_json::to_vec(&root).expect("a JSON value always serialises")
}

#[cfg(test)]
mod tests {
    use super::*;
    use minecraftoss_player::mining::MiningCatalog;

    #[test]
    fn catalog_parses_with_the_real_reader() {
        let catalog = MiningCatalog::from_slice(&mining_catalog_json()).unwrap();
        assert_eq!(catalog.block_count(), ROWS.len());
    }
    #[test]
    fn every_row_has_a_positive_hardness() {
        assert!(ROWS.iter().all(|r| r.hardness > 0.0));
    }
    #[test]
    fn curated_blocks_are_known_by_full_name() {
        assert!(is_curated_block("minecraft:obsidian"));
        assert!(!is_curated_block("obsidian"));
        assert!(!is_curated_block("minecraft:stick"));
    }
}
