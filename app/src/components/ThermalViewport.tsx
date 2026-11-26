import { type CSSProperties, useCallback, useEffect, useMemo, useRef, useState } from "react";
import { isTypingTarget } from "@/app/shortcuts";
import { type ColorRange, shownRange } from "@/lib/colorRange";
import type { FrameStore } from "@/lib/frameStore";
import { samplePalette } from "@/lib/palettes";
import {
  type DrawTool,
  type FrameSize,
  geometryFromDrag,
  type Handle,
  hitTest,
  moveGeometry,
  nextColorIndex,
  nextRegionName,
  type Pt,
  polygonStep,
  resizeGeometry,
  toPixel,
} from "@/lib/regions";
import {
  createViewportScratch,
  matchesIsotherm,
  objectCToRaw,
  prepareDisplay16Bit,
  rawToObjectC,
  type ViewportScratch,
} from "@/lib/thermal";
import type {
  DisplayPipeline,
  Palette,
  RadiometryParams,
  Roi,
  RoiGeometry,
  RoiTool,
  TempUnit,
  ThermalFrame,
} from "@/lib/types";
import { HoverLayer } from "./viewport/HoverLayer";
import { RegionLayer } from "./viewport/RegionLayer";

export interface ThermalViewportProps {
  /** Where frames come from; the viewer draws every frame pushed into it. */
  store: FrameStore;
  compareFrame?: ThermalFrame | null;
  splitRatio?: number;
  palette: Palette;
  range: ColorRange;
  rad: RadiometryParams;
  pipeline: DisplayPipeline;
  unit: TempUnit;
  rois: Roi[];
  selectedRoiId: string | null;
  /** Regions whose alarm is firing. */
  alertIds?: ReadonlySet<string>;
  tool: RoiTool;
  showHotCold: boolean;
  showGrid: boolean;
  gridCols: number;
  gridRows: number;
  zoom: number;
  /** Accessible name of the image. */
  label?: string;
  style?: CSSProperties;
  onRoisChange: (update: (rois: Roi[]) => Roi[]) => void;
  onSelectRoi: (id: string | null) => void;
  onToolChange: (tool: RoiTool) => void;
}

type Gesture =
  | { kind: "draw"; tool: DrawTool; a: Pt; b: Pt }
  | { kind: "move"; id: string; start: Pt; orig: RoiGeometry; cur: RoiGeometry }
  | { kind: "resize"; id: string; handle: Handle; orig: RoiGeometry; cur: RoiGeometry };

/** Screen pixels within which a pointer grabs a handle or a thin shape. */
const GRAB_PX = 7;

let regionSeq = 0;

const RESIZE_CURSORS: Record<string, string> = {
  nw: "nwse-resize",
  se: "nwse-resize",
  ne: "nesw-resize",
  sw: "nesw-resize",
  n: "ns-resize",
  s: "ns-resize",
  e: "ew-resize",
  w: "ew-resize",
};

function sizeOf(frame: ThermalFrame | null): FrameSize {
  return frame ? { width: frame.width, height: frame.height } : { width: 160, height: 120 };
}

function overlayOpen(): boolean {
  return Boolean(document.querySelector("[role=dialog], [role=menu]"));
}

const VERT_SRC = `#version 300 es
in vec2 a_pos;
out vec2 v_uv;
void main() {
  v_uv = a_pos * 0.5 + 0.5;
  v_uv.y = 1.0 - v_uv.y;
  gl_Position = vec4(a_pos, 0.0, 1.0);
}`;

const FRAG_SRC = `#version 300 es
precision highp float;
precision highp usampler2D;
in vec2 v_uv;
uniform usampler2D u_disp;
uniform usampler2D u_raw;
uniform sampler2D u_lut;
uniform vec2 u_texSize;
uniform int u_mode;
uniform int u_isoMode;
uniform float u_isoLow;
uniform float u_isoHigh;
uniform vec3 u_isoColor;
uniform vec2 u_flipRot;
out vec4 outColor;

const float PI = 3.14159265359;

float catmull(float x) {
  float ax = abs(x);
  if (ax < 1.0) return 1.5 * ax * ax * ax - 2.5 * ax * ax + 1.0;
  if (ax < 2.0) return -0.5 * ax * ax * ax + 2.5 * ax * ax - 4.0 * ax + 2.0;
  return 0.0;
}

float lanczos3(float x) {
  float ax = abs(x);
  if (ax < 1e-5) return 1.0;
  if (ax < 3.0) {
    float px = PI * ax;
    return (sin(px) / px) * (sin(px / 3.0) / (px / 3.0));
  }
  return 0.0;
}

float fetchNorm(usampler2D tex, ivec2 coord, ivec2 maxCoord) {
  ivec2 c = clamp(coord, ivec2(0), maxCoord);
  return float(texelFetch(tex, c, 0).r) / 65535.0;
}

float sampleBicubic(usampler2D tex, vec2 uv) {
  vec2 coord = uv * u_texSize - 0.5;
  vec2 base = floor(coord);
  vec2 f = coord - base;
  ivec2 ibase = ivec2(base);
  ivec2 maxC = ivec2(u_texSize) - ivec2(1);
  float sum = 0.0;
  float wsum = 0.0;
  for (int j = -1; j <= 2; j++) {
    float wy = catmull(f.y - float(j));
    for (int i = -1; i <= 2; i++) {
      float wx = catmull(f.x - float(i));
      float w = wx * wy;
      sum += fetchNorm(tex, ibase + ivec2(i, j), maxC) * w;
      wsum += w;
    }
  }
  return wsum > 1e-5 ? clamp(sum / wsum, 0.0, 1.0) : fetchNorm(tex, ibase, maxC);
}

float sampleLanczos3(usampler2D tex, vec2 uv) {
  vec2 coord = uv * u_texSize - 0.5;
  vec2 base = floor(coord);
  vec2 f = coord - base;
  ivec2 ibase = ivec2(base);
  ivec2 maxC = ivec2(u_texSize) - ivec2(1);
  float sum = 0.0;
  float wsum = 0.0;
  for (int j = -2; j <= 3; j++) {
    float wy = lanczos3(f.y - float(j));
    for (int i = -2; i <= 3; i++) {
      float wx = lanczos3(f.x - float(i));
      float w = wx * wy;
      sum += fetchNorm(tex, ibase + ivec2(i, j), maxC) * w;
      wsum += w;
    }
  }
  return wsum > 1e-5 ? clamp(sum / wsum, 0.0, 1.0) : fetchNorm(tex, ibase, maxC);
}

float sampleField(usampler2D tex, vec2 uv, int mode) {
  if (mode == 0) {
    ivec2 maxC = ivec2(u_texSize) - ivec2(1);
    return fetchNorm(tex, ivec2(floor(uv * u_texSize)), maxC);
  }
  if (mode == 1) {
    return sampleBicubic(tex, uv);
  }
  return sampleLanczos3(tex, uv);
}

void main() {
  vec2 uv = v_uv;
  if (u_flipRot.x > 0.5) uv = 1.0 - uv;
  if (u_flipRot.y > 0.5) uv.x = 1.0 - uv.x;

  float vDisp = sampleField(u_disp, uv, u_mode);

  // Isotherm ALWAYS evaluated against linear raw temperature before HEQ/USM!
  bool isoHit = false;
  if (u_isoMode > 0) {
    float vLin = sampleField(u_raw, uv, u_mode);
    if (u_isoMode == 1 && vLin >= u_isoHigh) isoHit = true;
    if (u_isoMode == 2 && vLin <= u_isoLow) isoHit = true;
    if (u_isoMode == 3 && vLin >= u_isoLow && vLin <= u_isoHigh) isoHit = true;
  }

  vec3 col = isoHit ? u_isoColor : texture(u_lut, vec2(vDisp, 0.5)).rgb;
  outColor = vec4(col, 1.0);
}`;

interface CachedUniforms {
  u_disp: WebGLUniformLocation | null;
  u_raw: WebGLUniformLocation | null;
  u_lut: WebGLUniformLocation | null;
  u_texSize: WebGLUniformLocation | null;
  u_mode: WebGLUniformLocation | null;
  u_isoMode: WebGLUniformLocation | null;
  u_isoLow: WebGLUniformLocation | null;
  u_isoHigh: WebGLUniformLocation | null;
  u_isoColor: WebGLUniformLocation | null;
  u_flipRot: WebGLUniformLocation | null;
}

interface WebGlHandles {
  gl: WebGL2RenderingContext;
  program: WebGLProgram;
  vbo: WebGLBuffer;
  dispTex: WebGLTexture;
  rawTex: WebGLTexture;
  lutTex: WebGLTexture;
  texW: number;
  texH: number;
  lutUploaded: Uint8Array | null;
  u: CachedUniforms;
}

function initWebGl2(canvas: HTMLCanvasElement): WebGlHandles | null {
  // No preserveDrawingBuffer: pictures are rendered in Rust, never read back from the screen.
  let gl: WebGL2RenderingContext | null = null;
  try {
    gl = canvas.getContext("webgl2", { antialias: false });
  } catch {
    gl = null;
  }
  if (!gl) return null;
  const compile = (type: number, src: string): WebGLShader | null => {
    const sh = gl.createShader(type);
    if (!sh) return null;
    gl.shaderSource(sh, src);
    gl.compileShader(sh);
    if (!gl.getShaderParameter(sh, gl.COMPILE_STATUS)) {
      gl.deleteShader(sh);
      return null;
    }
    return sh;
  };
  const vs = compile(gl.VERTEX_SHADER, VERT_SRC);
  const fs = compile(gl.FRAGMENT_SHADER, FRAG_SRC);
  if (!vs || !fs) return null;
  const program = gl.createProgram();
  if (!program) return null;
  gl.attachShader(program, vs);
  gl.attachShader(program, fs);
  gl.linkProgram(program);
  gl.deleteShader(vs);
  gl.deleteShader(fs);
  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
    gl.deleteProgram(program);
    return null;
  }

  const vbo = gl.createBuffer();
  if (!vbo) return null;
  gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]), gl.STATIC_DRAW);
  const loc = gl.getAttribLocation(program, "a_pos");
  gl.enableVertexAttribArray(loc);
  gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0);

  const dispTex = gl.createTexture();
  const rawTex = gl.createTexture();
  const lutTex = gl.createTexture();
  if (!dispTex || !rawTex || !lutTex) return null;

  for (const tex of [dispTex, rawTex]) {
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    // Integer R16UI textures require NEAREST filtering in WebGL2
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
  }
  gl.bindTexture(gl.TEXTURE_2D, lutTex);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 256, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);

  const u: CachedUniforms = {
    u_disp: gl.getUniformLocation(program, "u_disp"),
    u_raw: gl.getUniformLocation(program, "u_raw"),
    u_lut: gl.getUniformLocation(program, "u_lut"),
    u_texSize: gl.getUniformLocation(program, "u_texSize"),
    u_mode: gl.getUniformLocation(program, "u_mode"),
    u_isoMode: gl.getUniformLocation(program, "u_isoMode"),
    u_isoLow: gl.getUniformLocation(program, "u_isoLow"),
    u_isoHigh: gl.getUniformLocation(program, "u_isoHigh"),
    u_isoColor: gl.getUniformLocation(program, "u_isoColor"),
    u_flipRot: gl.getUniformLocation(program, "u_flipRot"),
  };

  // Bind sampler units once
  // biome-ignore lint/correctness/useHookAtTopLevel: WebGL2RenderingContext.useProgram is not a React hook
  gl.useProgram(program);
  gl.uniform1i(u.u_disp, 0);
  gl.uniform1i(u.u_raw, 1);
  gl.uniform1i(u.u_lut, 2);

  return {
    gl,
    program,
    vbo,
    dispTex,
    rawTex,
    lutTex,
    texW: 0,
    texH: 0,
    lutUploaded: null,
    u,
  };
}

function uploadR16(
  gl: WebGL2RenderingContext,
  unit: number,
  tex: WebGLTexture,
  realloc: boolean,
  w: number,
  h: number,
  data: Uint16Array,
): void {
  gl.activeTexture(unit);
  gl.bindTexture(gl.TEXTURE_2D, tex);
  if (realloc) {
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.R16UI, w, h, 0, gl.RED_INTEGER, gl.UNSIGNED_SHORT, data);
  } else {
    gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, w, h, gl.RED_INTEGER, gl.UNSIGNED_SHORT, data);
  }
}

interface PaintInputs {
  palette: Palette;
  lutBytes: Uint8Array;
  range: ColorRange;
  rad: RadiometryParams;
  pipeline: DisplayPipeline;
  compareFrame: ThermalFrame | null;
  splitRatio: number;
}

/**
 * The thermal image. It draws every frame straight from the store, outside React; regions sit
 * in their own layer and the pointer's work (drawing, the polygon's open edge) in another.
 */
export function ThermalViewport({
  store,
  compareFrame = null,
  splitRatio = 0.5,
  palette,
  range,
  rad,
  pipeline,
  unit,
  rois,
  selectedRoiId,
  alertIds,
  tool,
  showHotCold,
  showGrid,
  gridCols,
  gridRows,
  zoom,
  label = "Live thermal image",
  style,
  onRoisChange,
  onSelectRoi,
  onToolChange,
}: ThermalViewportProps): React.JSX.Element {
  const surfaceRef = useRef<HTMLDivElement | null>(null);
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const glRef = useRef<WebGlHandles | null>(null);
  const ctx2dRef = useRef<{ off: HTMLCanvasElement; img: ImageData } | null>(null);
  const scratchRef = useRef<ViewportScratch>(createViewportScratch(160 * 120));
  const [canvasSize, setCanvasSize] = useState<[number, number]>([960, 720]);
  const [size, setSize] = useState<FrameSize>(() => sizeOf(store.getFrame()));
  const [gesture, setGesture] = useState<Gesture | null>(null);
  const [draft, setDraft] = useState<Pt[]>([]);
  const [renamingId, setRenamingId] = useState<string | null>(null);

  const lutBytes = useMemo(() => {
    const out = new Uint8Array(256 * 4);
    for (let i = 0; i < 256; i++) {
      const [r, g, b] = samplePalette(palette, i / 255);
      out[i * 4] = r;
      out[i * 4 + 1] = g;
      out[i * 4 + 2] = b;
      out[i * 4 + 3] = 255;
    }
    return out;
  }, [palette]);

  // Everything the per-frame painter and the event handlers read, without re-subscribing.
  const paintRef = useRef<PaintInputs>({
    palette,
    lutBytes,
    range,
    rad,
    pipeline,
    compareFrame,
    splitRatio,
  });
  paintRef.current = { palette, lutBytes, range, rad, pipeline, compareFrame, splitRatio };
  const live = useRef({ rois, selectedRoiId, tool, gesture, draft, size, rad });
  live.current = { rois, selectedRoiId, tool, gesture, draft, size, rad };
  const handlers = useRef({ onRoisChange, onSelectRoi, onToolChange });
  handlers.current = { onRoisChange, onSelectRoi, onToolChange };

  // DPR-aware canvas backing store, resized only when the element's size changes.
  useEffect(() => {
    const cv = canvasRef.current;
    if (!cv) return;
    const updateSize = () => {
      const rect = cv.getBoundingClientRect();
      const dpr = Math.min(window.devicePixelRatio || 1, 3);
      const nextW = Math.max(320, Math.round(rect.width * dpr));
      const nextH = Math.max(240, Math.round(rect.height * dpr));
      setCanvasSize((prev) => (prev[0] === nextW && prev[1] === nextH ? prev : [nextW, nextH]));
    };
    updateSize();
    if (typeof ResizeObserver === "undefined") return;
    const ro = new ResizeObserver(updateSize);
    ro.observe(cv);
    return () => ro.disconnect();
  }, []);

  useEffect(() => {
    const cv = canvasRef.current;
    if (!cv) return;
    const init = () => {
      glRef.current = initWebGl2(cv);
    };
    init();
    const onLost = (e: Event) => {
      e.preventDefault();
      glRef.current = null;
    };
    cv.addEventListener("webglcontextlost", onLost);
    cv.addEventListener("webglcontextrestored", init);
    return () => {
      cv.removeEventListener("webglcontextlost", onLost);
      cv.removeEventListener("webglcontextrestored", init);
      const h = glRef.current;
      if (h) {
        h.gl.deleteTexture(h.dispTex);
        h.gl.deleteTexture(h.rawTex);
        h.gl.deleteTexture(h.lutTex);
        h.gl.deleteBuffer(h.vbo);
        h.gl.deleteProgram(h.program);
        glRef.current = null;
      }
    };
  }, []);

  /** Draws one frame with the current palette, range and pipeline. */
  const paint = useCallback((frame: ThermalFrame | null): boolean => {
    const cv = canvasRef.current;
    if (!cv || !frame) return false;
    const {
      lutBytes: lut,
      range: rng,
      rad: r,
      pipeline: pipe,
      compareFrame: cmp,
    } = paintRef.current;
    const sensorW = frame.width;
    const sensorH = frame.height;
    const [minRaw, maxRaw] = shownRange(rng, frame);
    const splitCol = cmp ? Math.round(sensorW * paintRef.current.splitRatio) : sensorW;
    const scratch = scratchRef.current;
    prepareDisplay16Bit(
      frame.pixels,
      cmp ? cmp.pixels : null,
      splitCol,
      sensorW,
      sensorH,
      minRaw,
      maxRaw,
      pipe,
      scratch,
    );

    const handles = glRef.current;
    if (handles) {
      const { gl, program, dispTex, rawTex, lutTex, u } = handles;
      gl.viewport(0, 0, cv.width, cv.height);
      // biome-ignore lint/correctness/useHookAtTopLevel: WebGL2RenderingContext.useProgram is not a React hook
      gl.useProgram(program);
      gl.pixelStorei(gl.UNPACK_ALIGNMENT, 2);
      const realloc = handles.texW !== sensorW || handles.texH !== sensorH;
      uploadR16(gl, gl.TEXTURE0, dispTex, realloc, sensorW, sensorH, scratch.disp16);
      uploadR16(gl, gl.TEXTURE1, rawTex, realloc, sensorW, sensorH, scratch.raw16);
      handles.texW = sensorW;
      handles.texH = sensorH;
      if (handles.lutUploaded !== lut) {
        gl.activeTexture(gl.TEXTURE2);
        gl.bindTexture(gl.TEXTURE_2D, lutTex);
        gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
        gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, 256, 1, gl.RGBA, gl.UNSIGNED_BYTE, lut);
        handles.lutUploaded = lut;
      }
      gl.uniform2f(u.u_texSize, sensorW, sensorH);
      const modeNum =
        pipe.upscaler === "nearest"
          ? 0
          : pipe.upscaler === "bicubic"
            ? 1
            : pipe.upscaler === "lanczos3"
              ? 2
              : 3;
      gl.uniform1i(u.u_mode, modeNum);
      gl.uniform2f(u.u_flipRot, pipe.rotate_180 ? 1 : 0, pipe.flip_horizontal ? 1 : 0);
      const rawSpan = Math.max(1, maxRaw - minRaw);
      const iso = pipe.isotherm;
      const isoModeNum =
        iso.mode === "off" ? 0 : iso.mode === "above" ? 1 : iso.mode === "below" ? 2 : 3;
      gl.uniform1i(u.u_isoMode, isoModeNum);
      gl.uniform1f(u.u_isoLow, (objectCToRaw(iso.low_c, r) - minRaw) / rawSpan);
      gl.uniform1f(u.u_isoHigh, (objectCToRaw(iso.high_c, r) - minRaw) / rawSpan);
      gl.uniform3f(u.u_isoColor, iso.color[0] / 255, iso.color[1] / 255, iso.color[2] / 255);
      gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
      return true;
    }

    // Canvas2D fallback, for systems without WebGL2.
    let ctx: CanvasRenderingContext2D | null = null;
    try {
      ctx = cv.getContext("2d");
    } catch {
      ctx = null;
    }
    if (!ctx) return false;
    let off = ctx2dRef.current;
    if (!off || off.img.width !== sensorW || off.img.height !== sensorH) {
      const canvas = document.createElement("canvas");
      canvas.width = sensorW;
      canvas.height = sensorH;
      const octx = canvas.getContext("2d");
      if (!octx) return false;
      off = { off: canvas, img: octx.createImageData(sensorW, sensorH) };
      ctx2dRef.current = off;
    }
    const octx = off.off.getContext("2d");
    if (!octx) return false;
    const data = off.img.data;
    const minC = rawToObjectC(minRaw, r);
    const maxC = rawToObjectC(maxRaw, r);
    for (let i = 0; i < scratch.disp16.length; i++) {
      const n = Math.min(255, scratch.disp16[i] >> 8);
      const tempC = minC + (scratch.raw16[i] / 65535) * (maxC - minC);
      let cr = lut[n * 4];
      let cg = lut[n * 4 + 1];
      let cb = lut[n * 4 + 2];
      if (matchesIsotherm(pipe.isotherm, tempC)) [cr, cg, cb] = pipe.isotherm.color;
      data[i * 4] = cr;
      data[i * 4 + 1] = cg;
      data[i * 4 + 2] = cb;
      data[i * 4 + 3] = 255;
    }
    octx.putImageData(off.img, 0, 0);
    ctx.imageSmoothingEnabled = pipe.upscaler !== "nearest";
    ctx.imageSmoothingQuality = "high";
    ctx.save();
    ctx.clearRect(0, 0, cv.width, cv.height);
    if (pipe.rotate_180) {
      ctx.translate(cv.width, cv.height);
      ctx.scale(-1, -1);
    }
    if (pipe.flip_horizontal) {
      ctx.translate(cv.width, 0);
      ctx.scale(-1, 1);
    }
    ctx.drawImage(off.off, 0, 0, cv.width, cv.height);
    ctx.restore();
    return true;
  }, []);

  // Every frame from the store, coalesced to the display's refresh, without React.
  useEffect(() => {
    let raf: number | null = null;
    const raq =
      typeof requestAnimationFrame === "function"
        ? requestAnimationFrame
        : (fn: FrameRequestCallback) =>
            setTimeout(() => fn(performance.now()), 16) as unknown as number;
    const caf =
      typeof cancelAnimationFrame === "function"
        ? cancelAnimationFrame
        : (id: number) => clearTimeout(id as unknown as ReturnType<typeof setTimeout>);
    const unsubscribe = store.subscribeFrame(() => {
      if (raf !== null) return;
      raf = raq(() => {
        raf = null;
        const fr = store.getFrame();
        if (!fr) return;
        const s = live.current.size;
        if (fr.width !== s.width || fr.height !== s.height) setSize(sizeOf(fr));
        if (paint(fr)) store.markDrawn();
      });
    });
    return () => {
      unsubscribe();
      if (raf !== null) caf(raf);
    };
  }, [store, paint]);

  // Redraw the current frame when what it is drawn with changes.
  // biome-ignore lint/correctness/useExhaustiveDependencies: these are the inputs paint() reads through its ref.
  useEffect(() => {
    const cv = canvasRef.current;
    if (cv && (cv.width !== canvasSize[0] || cv.height !== canvasSize[1])) {
      cv.width = canvasSize[0];
      cv.height = canvasSize[1];
    }
    paint(store.getFrame());
  }, [store, paint, canvasSize, lutBytes, range, rad, pipeline, compareFrame, splitRatio]);

  // ---------- Pointer work ----------

  const toSensor = (e: { clientX: number; clientY: number }): { p: Pt; tol: number } => {
    const el = surfaceRef.current;
    const { width: W, height: H } = live.current.size;
    if (!el) return { p: [0, 0], tol: 1 };
    const rect = el.getBoundingClientRect();
    const rw = rect.width || W;
    const rh = rect.height || H;
    const fx = ((e.clientX - rect.left) / rw) * W;
    const fy = ((e.clientY - rect.top) / rh) * H;
    const x = Number.isFinite(fx) ? Math.max(0, Math.min(W, fx)) : 0;
    const y = Number.isFinite(fy) ? Math.max(0, Math.min(H, fy)) : 0;
    return { p: [x, y], tol: Math.max(0.75, (GRAB_PX * W) / rw) };
  };

  const startGesture = useCallback((g: Gesture | null) => {
    live.current.gesture = g;
    setGesture(g);
  }, []);

  const setDraftPoints = useCallback((pts: Pt[]) => {
    live.current.draft = pts;
    setDraft(pts);
  }, []);

  const addRegion = (geometry: RoiGeometry) => {
    regionSeq += 1;
    const id = `${geometry.kind}_${Date.now().toString(36)}_${regionSeq}`;
    handlers.current.onRoisChange((prev) => [
      ...prev,
      {
        id,
        label: nextRegionName(geometry.kind, prev),
        color: nextColorIndex(prev),
        geometry,
        emissivity_override: null,
        reflected_temp_override_c: null,
        visible: true,
      },
    ]);
    handlers.current.onSelectRoi(id);
    handlers.current.onToolChange("select");
  };

  const updateHover = (p: Pt) => {
    const fr = store.getFrame();
    if (!fr) return;
    const [x, y] = toPixel(p, { width: fr.width, height: fr.height });
    const raw = fr.pixels[y * fr.width + x];
    store.setHover({ x, y, raw, tempC: rawToObjectC(raw, live.current.rad) });
  };

  const onPointerDown = (e: React.PointerEvent<HTMLDivElement>) => {
    if (e.button !== 0) return;
    const target = e.target instanceof Element ? e.target : null;
    if (target?.closest("input, button")) return;
    const { p, tol } = toSensor(e);
    const { rois: list, selectedRoiId: sel, tool: t, size: sz } = live.current;

    const labelId = target?.closest("[data-region-label]")?.getAttribute("data-region-label");
    const labelled = labelId ? list.find((r) => r.id === labelId) : undefined;
    if (labelled) {
      handlers.current.onSelectRoi(labelled.id);
      startGesture({
        kind: "move",
        id: labelled.id,
        start: p,
        orig: labelled.geometry,
        cur: labelled.geometry,
      });
      return;
    }

    if (t === "select") {
      const hit = hitTest(list, p, tol, sel);
      if (!hit) {
        handlers.current.onSelectRoi(null);
        return;
      }
      const roi = list.find((r) => r.id === hit.roiId);
      if (!roi) return;
      handlers.current.onSelectRoi(roi.id);
      startGesture(
        hit.part === "handle"
          ? {
              kind: "resize",
              id: roi.id,
              handle: hit.handle,
              orig: roi.geometry,
              cur: roi.geometry,
            }
          : { kind: "move", id: roi.id, start: p, orig: roi.geometry, cur: roi.geometry },
      );
      return;
    }
    if (t === "polygon") {
      const step = polygonStep(live.current.draft, { type: "click", pt: p }, tol, sz);
      setDraftPoints(step.draft);
      if (step.done) addRegion({ kind: "polygon", points: step.done });
      return;
    }
    if (t === "spot" || t === "spot3") {
      const g = geometryFromDrag(t, p, p, sz);
      if (g) addRegion(g);
      return;
    }
    startGesture({ kind: "draw", tool: t, a: p, b: p });
  };

  const onPointerMove = (e: React.PointerEvent<HTMLDivElement>) => {
    const { p, tol } = toSensor(e);
    updateHover(p);
    const g = live.current.gesture;
    const sz = live.current.size;
    // Capture only once a drag really moves, so a double-click still reaches the label it hit.
    if (g && e.buttons !== 0 && !e.currentTarget.hasPointerCapture?.(e.pointerId)) {
      try {
        e.currentTarget.setPointerCapture?.(e.pointerId);
      } catch {
        // Capture is a convenience; drags still work inside the image without it.
      }
    }
    if (g?.kind === "draw") {
      startGesture({ ...g, b: p });
      return;
    }
    if (g?.kind === "move") {
      startGesture({ ...g, cur: moveGeometry(g.orig, p[0] - g.start[0], p[1] - g.start[1], sz) });
      return;
    }
    if (g?.kind === "resize") {
      startGesture({ ...g, cur: resizeGeometry(g.orig, g.handle, p, sz) });
      return;
    }
    // No gesture: the cursor says what a press would do.
    const el = surfaceRef.current;
    if (!el) return;
    let cursor = live.current.tool === "select" ? "default" : "crosshair";
    if (live.current.tool === "select") {
      const hit = hitTest(live.current.rois, p, tol, live.current.selectedRoiId);
      if (hit?.part === "body") cursor = "move";
      else if (hit?.part === "handle")
        cursor = hit.handle.kind === "box" ? (RESIZE_CURSORS[hit.handle.h] ?? "move") : "grab";
    }
    if (el.style.cursor !== cursor) el.style.cursor = cursor;
  };

  const finishGesture = (e: React.PointerEvent<HTMLDivElement>) => {
    const g = live.current.gesture;
    try {
      e.currentTarget.releasePointerCapture?.(e.pointerId);
    } catch {
      // Released already.
    }
    if (!g) return;
    startGesture(null);
    if (g.kind === "draw") {
      const { p } = toSensor(e);
      const geom = geometryFromDrag(g.tool, g.a, p, live.current.size);
      if (geom) addRegion(geom);
      return;
    }
    if (JSON.stringify(g.cur) !== JSON.stringify(g.orig)) {
      handlers.current.onRoisChange((prev) =>
        prev.map((r) => (r.id === g.id ? { ...r, geometry: g.cur } : r)),
      );
    }
  };

  const onDoubleClick = (e: React.MouseEvent<HTMLDivElement>) => {
    const target = e.target instanceof Element ? e.target : null;
    if (target?.closest("input, button, [data-region-label]")) return;
    const { p, tol } = toSensor(e);
    const { tool: t, rois: list, selectedRoiId: sel, size: sz } = live.current;
    if (t === "polygon") {
      const step = polygonStep(live.current.draft, { type: "dblclick" }, tol, sz);
      setDraftPoints(step.draft);
      if (step.done) addRegion({ kind: "polygon", points: step.done });
      return;
    }
    if (t === "select") {
      const hit = hitTest(list, p, tol, sel);
      if (hit?.part === "body") setRenamingId(hit.roiId);
    }
  };

  // Delete removes the selected region; Esc cancels drawing, then the tool, then the selection.
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.isComposing || isTypingTarget(e.target) || overlayOpen()) return;
      if (e.altKey || e.metaKey || e.ctrlKey) return;
      const cur = live.current;
      if (e.key === "Escape") {
        if (cur.draft.length > 0 || cur.gesture) {
          setDraftPoints([]);
          startGesture(null);
        } else if (cur.tool !== "select") {
          handlers.current.onToolChange("select");
        } else if (cur.selectedRoiId) {
          handlers.current.onSelectRoi(null);
        } else {
          return;
        }
        e.preventDefault();
        return;
      }
      if ((e.key === "Delete" || e.key === "Backspace") && cur.selectedRoiId) {
        const id = cur.selectedRoiId;
        handlers.current.onRoisChange((prev) => prev.filter((r) => r.id !== id));
        handlers.current.onSelectRoi(null);
        e.preventDefault();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [setDraftPoints, startGesture]);

  // A polygon in progress is dropped when another tool is picked.
  useEffect(() => {
    if (tool !== "polygon" && live.current.draft.length > 0) setDraftPoints([]);
  }, [tool, setDraftPoints]);

  const shownRois = useMemo(() => {
    if (!gesture || gesture.kind === "draw") return rois;
    return rois.map((r) => (r.id === gesture.id ? { ...r, geometry: gesture.cur } : r));
  }, [rois, gesture]);

  const preview =
    gesture?.kind === "draw" ? geometryFromDrag(gesture.tool, gesture.a, gesture.b, size) : null;

  const onRename = useCallback((id: string, name: string | null) => {
    setRenamingId(null);
    const next = name?.trim();
    if (next) {
      handlers.current.onRoisChange((prev) =>
        prev.map((r) => (r.id === id ? { ...r, label: next } : r)),
      );
    }
  }, []);

  return (
    <div
      ref={surfaceRef}
      className="st-img"
      data-viewport-surface=""
      style={{ ...style, transform: zoom !== 1 ? `scale(${zoom})` : undefined }}
      onPointerDown={onPointerDown}
      onPointerMove={onPointerMove}
      onPointerUp={finishGesture}
      onPointerCancel={finishGesture}
      onPointerLeave={() => {
        if (!live.current.gesture) store.setHover(null);
      }}
      onDoubleClick={onDoubleClick}
    >
      <canvas
        ref={canvasRef}
        data-thermal-canvas="true"
        width={canvasSize[0]}
        height={canvasSize[1]}
        role="img"
        aria-label={label}
      />
      {compareFrame ? (
        <svg
          className="st-ov"
          viewBox={`0 0 ${size.width} ${size.height}`}
          preserveAspectRatio="none"
          aria-hidden="true"
        >
          <line
            className="w dash"
            x1={size.width * splitRatio}
            y1={0}
            x2={size.width * splitRatio}
            y2={size.height}
          />
        </svg>
      ) : null}
      <RegionLayer
        store={store}
        size={size}
        rois={shownRois}
        selectedRoiId={selectedRoiId}
        alertIds={alertIds}
        rad={rad}
        unit={unit}
        showHotCold={showHotCold}
        showGrid={showGrid}
        gridCols={gridCols}
        gridRows={gridRows}
        renamingId={renamingId}
        onStartRename={setRenamingId}
        onRename={onRename}
      />
      <HoverLayer store={store} size={size} preview={preview} polygonDraft={draft} />
    </div>
  );
}
