import { demoSceneFrame } from "./demoScenes";
import { generateDemoFrame } from "./thermal";
import type { ThermalFrame } from "./types";

/**
 * Where the demo camera's image comes from in the browser preview: frame `seq` of the eight
 * real sample captures (`demoScenes.ts`), the same ones the Tauri app's Rust demo camera
 * plays. The generated scene stands in until they have loaded.
 */
export function demoCameraFrame(seq: number): ThermalFrame {
  return demoSceneFrame(seq) ?? generateDemoFrame(seq);
}

/** A saved demo capture's frame: drawn from its own `seq`, never from a held scene. */
export function demoCaptureFrame(seq: number): ThermalFrame {
  return demoSceneFrame(seq, false) ?? generateDemoFrame(seq);
}
