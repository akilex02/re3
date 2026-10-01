//! Item ids shared with the C++ item table (index = id, 0 = air).
use std::collections::HashMap;

pub struct Names {
    by_id: Vec<String>,
    by_name: HashMap<String, i32>,
    last_block: i32,
}

impl Names {
    /// `names[0]` is air. Ids `1..=last_block` are placeable blocks.
    pub fn new(names: Vec<String>, last_block: i32) -> Self {
        let by_name = names
            .iter()
            .enumerate()
            .map(|(i, n)| (n.clone(), i as i32))
            .collect();
        Self {
            by_id: names,
            by_name,
            last_block,
        }
    }
    pub fn name(&self, id: i32) -> Option<&str> {
        if id <= 0 {
            return None;
        }
        self.by_id.get(id as usize).map(String::as_str)
    }
    pub fn id(&self, name: &str) -> Option<i32> {
        self.by_name.get(name).copied().filter(|&i| i > 0)
    }
    pub fn is_block(&self, id: i32) -> bool {
        id >= 1 && id <= self.last_block && (id as usize) < self.by_id.len()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn sample() -> Names {
        Names::new(
            vec![
                "minecraft:air".into(),
                "minecraft:dirt".into(),
                "minecraft:stick".into(),
            ],
            1,
        )
    }
    #[test]
    fn lookup_both_ways() {
        let n = sample();
        assert_eq!(n.name(1), Some("minecraft:dirt"));
        assert_eq!(n.id("minecraft:stick"), Some(2));
    }
    #[test]
    fn air_unknown_and_negative_are_none() {
        let n = sample();
        assert_eq!(n.name(0), None);
        assert_eq!(n.name(-3), None);
        assert_eq!(n.name(99), None);
        assert_eq!(n.id("minecraft:air"), None);
        assert_eq!(n.id("minecraft:nope"), None);
    }
    #[test]
    fn blocks_versus_items() {
        let n = sample();
        assert!(n.is_block(1));
        assert!(!n.is_block(2));
        assert!(!n.is_block(0));
    }
}
