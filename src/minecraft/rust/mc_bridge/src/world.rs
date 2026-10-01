//! The C++ voxel grid seen as a Minecraft `World`: cells go through `coords`, ids through `Names`.
use crate::coords::to_gta_cell;
use crate::names::Names;
use minecraftoss_player::{Block, Pos, World};
use std::os::raw::c_void;

pub type GetFn = unsafe extern "C" fn(*mut c_void, i32, i32, i32) -> i32;
pub type SetFn = unsafe extern "C" fn(*mut c_void, i32, i32, i32, i32);

pub struct CWorld<'a> {
    get: GetFn,
    set: SetFn,
    ctx: *mut c_void,
    names: &'a Names,
}

impl<'a> CWorld<'a> {
    /// # Safety
    /// `get` and `set` must be safe to call with `ctx` for as long as this value lives.
    pub unsafe fn new(get: GetFn, set: SetFn, ctx: *mut c_void, names: &'a Names) -> Self {
        Self {
            get,
            set,
            ctx,
            names,
        }
    }
}

impl World for CWorld<'_> {
    fn block(&self, pos: Pos) -> Option<Block> {
        let g = to_gta_cell(pos);
        // SAFETY: guaranteed by the caller of `CWorld::new`.
        let id = unsafe { (self.get)(self.ctx, g[0], g[1], g[2]) };
        if !self.names.is_block(id) {
            return None;
        }
        self.names.name(id).map(Block::new)
    }
    fn set_block(&mut self, pos: Pos, block: Option<Block>) {
        let g = to_gta_cell(pos);
        let id = block.and_then(|b| self.names.id(&b.id)).unwrap_or(0);
        // SAFETY: guaranteed by the caller of `CWorld::new`.
        unsafe { (self.set)(self.ctx, g[0], g[1], g[2], id) };
    }
}
