import { generateDemoFrame } from "./thermal";
import type { ThermalFrame } from "./types";

/**
 * Where the demo camera's image comes from in the browser preview: frame `seq` of its scene.
 * The Tauri app's demo camera draws the same scene in Rust (`generate_demo_frame`).
 *
 * This is the one place to change if the demo camera should play sample `.DAT` captures
 * instead of the generated scene.
 */
export function demoCameraFrame(seq: number): ThermalFrame {
  return generateDemoFrame(seq);
}
