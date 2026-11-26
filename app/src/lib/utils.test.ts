import { describe, expect, test } from "bun:test";
import { rawBytes } from "./utils";

describe("raw bytes from a Rust command", () => {
  const png = [137, 80, 78, 71, 13, 10, 26, 10];

  test("arrive as an ArrayBuffer over Tauri's IPC channel", () => {
    const buf = Uint8Array.from(png).buffer;
    expect([...new Uint8Array(rawBytes(buf))]).toEqual(png);
  });

  test("arrive as a list of numbers when Tauri falls back to postMessage", () => {
    // A Blob made from this list directly would hold the text "137,80,…", a broken picture.
    expect([...new Uint8Array(rawBytes(png))]).toEqual(png);
  });

  test("arrive as a view on a larger buffer", () => {
    const big = Uint8Array.from([0, 0, ...png, 0]);
    expect([...new Uint8Array(rawBytes(big.subarray(2, 10)))]).toEqual(png);
  });
});
