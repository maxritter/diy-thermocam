import { describe, expect, test } from "bun:test";
import { DEFAULT_PALETTES, parseCustomPaletteText, samplePalette } from "./palettes";
import {
  applyClippedHistogramEq,
  compensateEmissivityC,
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
    expect(Math.abs(compensateEmissivityC(50.0, 1.0, 22.0) - 50.0)).toBeLessThan(1e-4);
    const corrected = compensateEmissivityC(50.0, 0.5, 22.0);
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

  test("binary FrameRaw packet decoding from Tauri Channel<InvokeResponseBody::Raw>", () => {
    const w = 4;
    const h = 3;
    const buf = new ArrayBuffer(64 + w * h * 2);
    const view = new DataView(buf);
    view.setUint32(0, 42, true);
    view.setBigUint64(4, 123456n, true);
    view.setUint16(18, w, true);
    view.setUint16(20, h, true);
    view.setUint16(26, 29500, true);
    view.setUint16(28, 32500, true);
    view.setFloat32(42, -273.15, true);
    view.setFloat32(46, 0.01, true);
    view.setInt8(54, 88);
    view.setUint8(55, 1);
    for (let i = 0; i < w * h; i++) {
      view.setUint16(64 + i * 2, 30000 + i * 100, true);
    }
    const decoded = decodeFrameRawBinary(buf);
    expect(decoded.frame_seq).toBe(42);
    expect(decoded.width).toBe(4);
    expect(decoded.height).toBe(3);
    expect(decoded.bat_pct).toBe(88);
    expect(decoded.pixels[0]).toBe(30000);
    expect(decoded.pixels[11]).toBe(31100);
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
