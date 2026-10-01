//! GTA (x east, y north, z up) <-> Minecraft (x east, y up, z south).
use glam::DVec3;

pub fn to_mc_point(g: [f64; 3]) -> DVec3 {
    DVec3::new(g[0], g[2], -g[1])
}
pub fn to_mc_cell(g: [i32; 3]) -> (i32, i32, i32) {
    (g[0], g[2], -g[1] - 1)
}
pub fn to_gta_cell(m: (i32, i32, i32)) -> [i32; 3] {
    [m.0, -m.2 - 1, m.1]
}
/// Minecraft yaw/pitch in degrees for a GTA direction (0 yaw south, positive pitch looks down).
pub fn yaw_pitch(dir: [f64; 3]) -> (f64, f64) {
    let m = to_mc_point(dir).normalize_or_zero();
    ((-m.x).atan2(m.z).to_degrees(), (-m.y).clamp(-1.0, 1.0).asin().to_degrees())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn cells_round_trip_including_negatives() {
        for g in [[0, 0, 0], [5, -7, 3], [-1, -1, -1], [100, 200, 50]] {
            assert_eq!(to_gta_cell(to_mc_cell(g)), g);
        }
    }
    #[test]
    fn cell_contains_its_point() {
        // point inside GTA cell (2,3,4) at (2.5,3.5,4.5) lands in the mapped MC cell
        let p = to_mc_point([2.5, 3.5, 4.5]);
        let c = to_mc_cell([2, 3, 4]);
        assert_eq!((p.x.floor() as i32, p.y.floor() as i32, p.z.floor() as i32), c);
    }
    #[test]
    fn directions_map_to_minecraft_yaw() {
        let (north, _) = yaw_pitch([0.0, 1.0, 0.0]);
        assert!((north.abs() - 180.0).abs() < 1e-9);
        let (east, _) = yaw_pitch([1.0, 0.0, 0.0]);
        assert!((east + 90.0).abs() < 1e-9);
        let (_, down) = yaw_pitch([0.0, 0.0, -1.0]);
        assert!((down - 90.0).abs() < 1e-9);
        let (_, up) = yaw_pitch([0.0, 0.0, 1.0]);
        assert!((up + 90.0).abs() < 1e-9);
    }
    #[test]
    fn zero_direction_does_not_panic() {
        let _ = yaw_pitch([0.0, 0.0, 0.0]);
    }
}
