import { describe, expect, test } from "bun:test";
import goldenFrameVectors from "../../fixtures/frame_raw_golden.json";
import { DEFAULT_PALETTES, parseCustomPaletteText, samplePalette } from "./palettes";
import {
  applyClippedHistogramEq,
  computeHistogram256,
  computeTempGrid,
  createViewportScratch,
  decodeFrameRawBinary,
  evaluateRoi,
  formatDeltaTemp,
  formatTemp,
  generateDemoFrame,
  objectCToRaw,
  prepareDisplay16Bit,
  rawToObjectC,
  readingC,
  roiContrastRange,
  sampleLineProfile,
} from "./thermal";
import type { DisplayPipeline, RadiometryParams, Roi } from "./types";

describe("DIY-Thermocam frontend thermal engine", () => {
  const rad: RadiometryParams = {
    cal_offset: -273.15,
    cal_slope: 0.01,
    emissivity: 0.95,
    reflected_temp_c: 22.0,
  };

  test("Stefan-Boltzmann emissivity compensation & unit formatting", () => {
    // 32315 counts read 50.0 °C apparent at 0.01 K per count.
    const cal = { cal_offset: -273.15, cal_slope: 0.01 };
    const blackbody = { emissivity: 1.0, reflected_c: 22.0 };
    expect(Math.abs(readingC(32315, cal, null, blackbody) - 50.0)).toBeLessThan(1e-4);
    const corrected = readingC(32315, cal, null, { emissivity: 0.5, reflected_c: 22.0 });
    expect(corrected).toBeGreaterThan(70.0);
    expect(corrected).toBeLessThan(85.0);
    expect(formatTemp(25.0, "celsius", 1)).toBe("25.0 °C");
    expect(formatTemp(25.0, "fahrenheit", 1)).toBe("77.0 °F");
    expect(formatTemp(0.0, "kelvin", 2)).toBe("273.15 K");
    expect(formatDeltaTemp(10.0, "fahrenheit", 1)).toBe("+18.0 °F");
  });

  test("demo frame generation, ROI evaluation, contrast range, line profile, grid & histogram", () => {
    const frame = generateDemoFrame(3, 160, 120);
    expect(frame.pixels.length).toBe(19200);
    expect(frame.max_raw).toBeGreaterThan(frame.min_raw);

    const boxRoi: Roi = {
      id: "b1",
      label: "MCU",
      geometry: { kind: "box", x: 45, y: 45, w: 30, h: 25 },
      emissivity_override: 0.91,
      reflected_temp_override_c: null,
      visible: true,
    };
    const stats = evaluateRoi(boxRoi, frame.pixels, 160, 120, rad);
    expect(stats.pixel_count).toBe(750);
    expect(stats.effective_emissivity).toBeCloseTo(0.91, 4);
    expect(stats.max_c).toBeGreaterThan(stats.min_c);

    const range = roiContrastRange(boxRoi, frame.pixels, 160, 120, 0.05);
    expect(range).not.toBeNull();
    if (range) expect(range[1]).toBeGreaterThan(range[0]);

    const line = sampleLineProfile(frame.pixels, 160, 120, 10, 60, 150, 60, rad);
    expect(line.length).toBe(141);

    const grid = computeTempGrid(frame.pixels, 160, 120, 4, 3, rad);
    expect(grid.length).toBe(3);
    expect(grid[0].length).toBe(4);

    const hist = computeHistogram256(frame.pixels, frame.min_raw, frame.max_raw);
    const total = Array.from(hist).reduce((a, b) => a + b, 0);
    expect(total).toBe(19200);

    const lowContrast = new Uint8Array(64);
    for (let i = 0; i < 64; i++) lowContrast[i] = 100 + (i % 16);
    const eq = applyClippedHistogramEq(lowContrast);
    expect(eq.length).toBe(64);
    expect(Math.max(...eq) - Math.min(...eq)).toBeGreaterThan(100);
  });

  test("binary FrameRaw packet decoding matches Protocol v2 §4.3 (64-byte header + u16 pixels)", () => {
    const w = 4;
    const h = 3;
    const buf = new ArrayBuffer(64 + w * h * 2);
    const view = new DataView(buf);
    view.setUint32(0, 42, true);
    view.setBigUint64(4, 123456n, true);
    view.setUint32(12, 1764066000, true);
    view.setUint32(16, 7, true); // dropped_frames: u32 (4 bytes)
    view.setUint16(20, w, true);
    view.setUint16(22, h, true);
    view.setUint8(24, 0); // pixel_format
    view.setUint8(25, 0); // gain_mode
    view.setUint8(26, 0); // ffc_state
    view.setUint8(27, 0x02); // flags
    view.setUint16(28, 29500, true);
    view.setUint16(30, 32500, true);
    view.setUint16(32, 1, true);
    view.setUint16(34, 2, true);
    view.setUint16(36, 3, true);
    view.setUint16(38, 0, true);
    view.setFloat32(40, 28.5, true);
    view.setFloat32(44, -273.15, true);
    view.setFloat32(48, 0.01, true);
    view.setFloat32(52, 31.4, true);
    view.setInt8(56, 88);
    view.setUint8(57, 1);
    for (let i = 0; i < w * h; i++) {
      view.setUint16(64 + i * 2, 30000 + i * 100, true);
    }
    const decoded = decodeFrameRawBinary(buf);
    expect(decoded.frame_seq).toBe(42);
    expect(decoded.timestamp_us).toBe(123456);
    expect(decoded.rtc_epoch).toBe(1764066000);
    expect(decoded.dropped_frames).toBe(7);
    expect(decoded.width).toBe(4);
    expect(decoded.height).toBe(3);
    expect(decoded.min_raw).toBe(29500);
    expect(decoded.max_raw).toBe(32500);
    expect(decoded.min_x).toBe(1);
    expect(decoded.min_y).toBe(2);
    expect(decoded.max_x).toBe(3);
    expect(decoded.max_y).toBe(0);
    expect(decoded.spot_temp_c).toBeCloseTo(28.5, 4);
    expect(decoded.cal_offset).toBeCloseTo(-273.15, 3);
    expect(decoded.cal_slope).toBeCloseTo(0.01, 5);
    expect(decoded.fpa_temp_c).toBeCloseTo(31.4, 3);
    expect(decoded.bat_pct).toBe(88);
    expect(decoded.usb_power).toBe(true);
    expect(decoded.pixels[0]).toBe(30000);
    expect(decoded.pixels[11]).toBe(31100);
  });

  test("cross-language golden FrameRaw binary vectors match Rust FrameRaw::to_bytes byte-for-byte", () => {
    expect(goldenFrameVectors.cases.length).toBeGreaterThanOrEqual(2);
    for (const vec of goldenFrameVectors.cases) {
      const hex = vec.wire_hex;
      expect(hex.length % 2).toBe(0);
      const bytes = new Uint8Array(hex.length / 2);
      for (let i = 0; i < bytes.length; i++) {
        bytes[i] = Number.parseInt(hex.slice(i * 2, i * 2 + 2), 16);
      }
      const decoded = decodeFrameRawBinary(bytes.buffer);
      expect(decoded.frame_seq).toBe(vec.frame_seq);
      expect(decoded.timestamp_us).toBe(vec.timestamp_us);
      expect(decoded.rtc_epoch).toBe(vec.rtc_epoch);
      expect(decoded.dropped_frames).toBe(vec.dropped_frames);
      expect(decoded.width).toBe(vec.width);
      expect(decoded.height).toBe(vec.height);
      expect(decoded.min_raw).toBe(vec.min_raw);
      expect(decoded.max_raw).toBe(vec.max_raw);
      expect(decoded.min_x).toBe(vec.min_x);
      expect(decoded.min_y).toBe(vec.min_y);
      expect(decoded.max_x).toBe(vec.max_x);
      expect(decoded.max_y).toBe(vec.max_y);
      expect(decoded.spot_temp_c).toBeCloseTo(vec.spot_temp_c, 4);
      expect(decoded.cal_offset).toBeCloseTo(vec.cal_offset, 3);
      expect(decoded.cal_slope).toBeCloseTo(vec.cal_slope, 5);
      expect(decoded.fpa_temp_c).toBeCloseTo(vec.fpa_temp_c, 3);
      expect(decoded.bat_pct).toBe(vec.bat_pct);
      expect(decoded.usb_power).toBe(vec.usb_power);
      expect(Array.from(decoded.pixels)).toEqual(vec.pixels);
    }
  });

  test("palettes & custom 256-entry LUT parser", () => {
    expect(DEFAULT_PALETTES.length).toBeGreaterThanOrEqual(12);
    const inferno = DEFAULT_PALETTES.find((p) => p.id === "inferno");
    expect(inferno).toBeDefined();
    if (inferno) {
      const c = samplePalette(inferno, 0.5);
      expect(c.length).toBe(3);
    }
    const custom = parseCustomPaletteText("MyRamp", "#000000\n#ff8000\n#ffffff");
    expect(custom.rgb.length).toBe(256);
    expect(custom.rgb[0]).toEqual([0, 0, 0]);
    expect(custom.rgb[255]).toEqual([255, 255, 255]);
  });

  test("16-bit WebGL2 pipeline parity: objectCToRaw inverse, raw16 vs HEQ disp16, and halo-clamped USM", () => {
    const lowEpsRad: RadiometryParams = {
      cal_offset: -273.15,
      cal_slope: 0.01,
      emissivity: 0.65,
      reflected_temp_c: 22.0,
    };
    const rawIn = 32500;
    const tempC = rawToObjectC(rawIn, lowEpsRad);
    const rawBack = objectCToRaw(tempC, lowEpsRad);
    expect(Math.abs(rawBack - rawIn)).toBeLessThan(0.05);

    const frame = generateDemoFrame(4, 160, 120);
    const scratch = createViewportScratch(160 * 120);
    const pipeline: DisplayPipeline = {
      filter: "none",
      upscaler: "edge_preserve",
      sharpen_amount: 0.9,
      rotate_180: false,
      flip_horizontal: false,
      histogram_eq: true,
      isotherm: { mode: "above", low_c: 40, high_c: 60, color: [0, 230, 160] },
    };

    prepareDisplay16Bit(
      frame.pixels,
      null,
      160,
      160,
      120,
      frame.min_raw,
      frame.max_raw,
      pipeline,
      scratch,
    );

    // raw16 preserves linear normalized values (for isotherm before HEQ) while disp16 applies HEQ+USM
    let diffCount = 0;
    let fine16BitCount = 0;
    for (let i = 0; i < scratch.raw16.length; i++) {
      if (scratch.raw16[i] !== scratch.disp16[i]) diffCount++;
      if ((scratch.raw16[i] & 0xff) !== 0) fine16BitCount++;
    }
    expect(diffCount).toBeGreaterThan(1000);
    expect(fine16BitCount).toBeGreaterThan(1000);
    expect(scratch.heq.smoothedCdf).not.toBeNull();
  });
});
