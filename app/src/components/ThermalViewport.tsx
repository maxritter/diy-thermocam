import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { samplePalette } from "@/lib/palettes";
import {
  applyClippedHistogramEq,
  evaluateRoi,
  formatTemp,
  matchesIsotherm,
  rawToObjectC,
  unitSymbol,
} from "@/lib/thermal";
import type {
  DisplayPipeline,
  Palette,
  RadiometryParams,
  Roi,
  RoiTool,
  TempUnit,
  ThermalFrame,
} from "@/lib/types";

const LOUPE_CELLS = Array.from({ length: 49 }, (_, idx) => {
  const dx = (idx % 7) - 3;
  const dy = Math.floor(idx / 7) - 3;
  return { key: `lp_${dx}_${dy}`, dx, dy, isCenter: dx === 0 && dy === 0 };
});

interface ThermalViewportProps {
  frame: ThermalFrame;
  compareFrame?: ThermalFrame | null;
  splitRatio?: number;
  palette: Palette;
  minRaw: number;
  maxRaw: number;
  rad: RadiometryParams;
  pipeline: DisplayPipeline;
  unit: TempUnit;
  rois: Roi[];
  selectedRoiId: string | null;
  activeTool: RoiTool;
  showHotCold: boolean;
  showLoupe: boolean;
  showGridOverlay: boolean;
  gridCols: number;
  gridRows: number;
  zoom: number;
  onSelectRoi: (id: string | null) => void;
  onAddRoi: (roi: Roi) => void;
  onHoverPixel?: (info: { x: number; y: number; raw: number; tempC: number } | null) => void;
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
in vec2 v_uv;
uniform sampler2D u_norm;
uniform sampler2D u_lut;
uniform vec2 u_texSize;
uniform int u_mode;
uniform float u_sharpen;
uniform int u_isoMode;
uniform float u_isoLow;
uniform float u_isoHigh;
uniform vec3 u_isoColor;
uniform vec2 u_flipRot;
out vec4 outColor;

float catmull(float x) {
  float ax = abs(x);
  if (ax < 1.0) return 1.5 * ax * ax * ax - 2.5 * ax * ax + 1.0;
  if (ax < 2.0) return -0.5 * ax * ax * ax + 2.5 * ax * ax - 4.0 * ax + 2.0;
  return 0.0;
}

float sampleSmooth(vec2 uv) {
  vec2 coord = uv * u_texSize - 0.5;
  vec2 base = floor(coord);
  vec2 f = coord - base;
  float sum = 0.0;
  float wsum = 0.0;
  for (int j = -1; j <= 2; j++) {
    float wy = catmull(f.y - float(j));
    for (int i = -1; i <= 2; i++) {
      float wx = catmull(f.x - float(i));
      float w = wx * wy;
      vec2 p = clamp((base + vec2(float(i), float(j)) + 0.5) / u_texSize, vec2(0.0), vec2(1.0));
      sum += texture(u_norm, p).r * w;
      wsum += w;
    }
  }
  return wsum > 1e-5 ? clamp(sum / wsum, 0.0, 1.0) : texture(u_norm, uv).r;
}

void main() {
  vec2 uv = v_uv;
  if (u_flipRot.x > 0.5) uv = 1.0 - uv;
  if (u_flipRot.y > 0.5) uv.x = 1.0 - uv.x;

  float v = (u_mode == 0) ? texture(u_norm, (floor(uv * u_texSize) + 0.5) / u_texSize).r : sampleSmooth(uv);
  if (u_mode > 0 && u_sharpen > 0.001) {
    vec2 px = 1.0 / u_texSize;
    float blur = (
      texture(u_norm, uv + vec2(-px.x, 0.0)).r +
      texture(u_norm, uv + vec2( px.x, 0.0)).r +
      texture(u_norm, uv + vec2(0.0, -px.y)).r +
      texture(u_norm, uv + vec2(0.0,  px.y)).r +
      v * 2.0
    ) / 6.0;
    v = clamp(v + u_sharpen * 0.65 * (v - blur), 0.0, 1.0);
  }

  bool isoHit = false;
  if (u_isoMode == 1 && v >= u_isoHigh) isoHit = true;
  if (u_isoMode == 2 && v <= u_isoLow) isoHit = true;
  if (u_isoMode == 3 && v >= u_isoLow && v <= u_isoHigh) isoHit = true;

  vec3 col = isoHit ? u_isoColor : texture(u_lut, vec2(v, 0.5)).rgb;
  outColor = vec4(col, 1.0);
}`;

interface WebGlHandles {
  gl: WebGL2RenderingContext;
  program: WebGLProgram;
  normTex: WebGLTexture;
  lutTex: WebGLTexture;
}

function initWebGl2(canvas: HTMLCanvasElement): WebGlHandles | null {
  const gl = canvas.getContext("webgl2", { preserveDrawingBuffer: true, antialias: false });
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
  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) return null;

  const buf = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, buf);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]), gl.STATIC_DRAW);
  const loc = gl.getAttribLocation(program, "a_pos");
  gl.enableVertexAttribArray(loc);
  gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0);

  const normTex = gl.createTexture();
  const lutTex = gl.createTexture();
  if (!normTex || !lutTex) return null;

  for (const tex of [normTex, lutTex]) {
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  }
  return { gl, program, normTex, lutTex };
}

export function ThermalViewport({
  frame,
  compareFrame,
  splitRatio = 0.5,
  palette,
  minRaw,
  maxRaw,
  rad,
  pipeline,
  unit,
  rois,
  selectedRoiId,
  activeTool,
  showHotCold,
  showLoupe,
  showGridOverlay,
  gridCols,
  gridRows,
  zoom,
  onSelectRoi,
  onAddRoi,
  onHoverPixel,
}: ThermalViewportProps): React.JSX.Element {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const glRef = useRef<WebGlHandles | null>(null);
  const [backendMode, setBackendMode] = useState<"WebGL2" | "Canvas2D">("WebGL2");
  const [hover, setHover] = useState<{ x: number; y: number; raw: number; tempC: number } | null>(
    null,
  );
  const [dragStart, setDragStart] = useState<[number, number] | null>(null);
  const [dragCur, setDragCur] = useState<[number, number] | null>(null);
  const [polyDraft, setPolyDraft] = useState<[number, number][]>([]);

  const sensorW = frame.width;
  const sensorH = frame.height;

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

  useEffect(() => {
    const cv = canvasRef.current;
    if (!cv || glRef.current) return;
    const handles = initWebGl2(cv);
    if (handles) {
      glRef.current = handles;
      setBackendMode("WebGL2");
    } else {
      setBackendMode("Canvas2D");
    }
  }, []);

  useEffect(() => {
    const cv = canvasRef.current;
    if (!cv) return;
    const span = Math.max(1, maxRaw - minRaw);
    const rawNormBytes = new Uint8Array(sensorW * sensorH);
    const splitCol = compareFrame ? Math.round(sensorW * splitRatio) : sensorW;

    for (let y = 0; y < sensorH; y++) {
      for (let x = 0; x < sensorW; x++) {
        const idx = y * sensorW + x;
        const raw =
          compareFrame && x >= splitCol
            ? (compareFrame.pixels[idx] ?? frame.pixels[idx])
            : frame.pixels[idx];
        const norm = Math.max(0, Math.min(1, (raw - minRaw) / span));
        rawNormBytes[idx] = Math.round(norm * 255);
      }
    }
    const normBytes = pipeline.histogram_eq ? applyClippedHistogramEq(rawNormBytes) : rawNormBytes;

    const handles = glRef.current;
    if (handles) {
      const { gl, program, normTex, lutTex } = handles;
      gl.viewport(0, 0, cv.width, cv.height);
      // biome-ignore lint/correctness/useHookAtTopLevel: WebGL2RenderingContext.useProgram is not a React hook
      gl.useProgram(program);

      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, normTex);
      gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
      gl.texImage2D(
        gl.TEXTURE_2D,
        0,
        gl.R8,
        sensorW,
        sensorH,
        0,
        gl.RED,
        gl.UNSIGNED_BYTE,
        normBytes,
      );
      gl.uniform1i(gl.getUniformLocation(program, "u_norm"), 0);

      gl.activeTexture(gl.TEXTURE1);
      gl.bindTexture(gl.TEXTURE_2D, lutTex);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 256, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, lutBytes);
      gl.uniform1i(gl.getUniformLocation(program, "u_lut"), 1);

      gl.uniform2f(gl.getUniformLocation(program, "u_texSize"), sensorW, sensorH);
      gl.uniform1i(
        gl.getUniformLocation(program, "u_mode"),
        pipeline.upscaler === "nearest" ? 0 : pipeline.upscaler === "edge_preserve" ? 2 : 1,
      );
      gl.uniform1f(gl.getUniformLocation(program, "u_sharpen"), pipeline.sharpen_amount);
      gl.uniform2f(
        gl.getUniformLocation(program, "u_flipRot"),
        pipeline.rotate_180 ? 1 : 0,
        pipeline.flip_horizontal ? 1 : 0,
      );

      const minC = rawToObjectC(minRaw, rad);
      const maxC = rawToObjectC(maxRaw, rad);
      const cSpan = Math.max(0.1, maxC - minC);
      const isoModeNum =
        pipeline.isotherm.mode === "off"
          ? 0
          : pipeline.isotherm.mode === "above"
            ? 1
            : pipeline.isotherm.mode === "below"
              ? 2
              : 3;
      gl.uniform1i(gl.getUniformLocation(program, "u_isoMode"), isoModeNum);
      gl.uniform1f(
        gl.getUniformLocation(program, "u_isoLow"),
        (pipeline.isotherm.low_c - minC) / cSpan,
      );
      gl.uniform1f(
        gl.getUniformLocation(program, "u_isoHigh"),
        (pipeline.isotherm.high_c - minC) / cSpan,
      );
      gl.uniform3f(
        gl.getUniformLocation(program, "u_isoColor"),
        pipeline.isotherm.color[0] / 255,
        pipeline.isotherm.color[1] / 255,
        pipeline.isotherm.color[2] / 255,
      );
      gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
      return;
    }

    const ctx = cv.getContext("2d");
    if (!ctx) return;
    const off = document.createElement("canvas");
    off.width = sensorW;
    off.height = sensorH;
    const octx = off.getContext("2d");
    if (!octx) return;
    const imgData = octx.createImageData(sensorW, sensorH);
    const minC = rawToObjectC(minRaw, rad);
    const maxC = rawToObjectC(maxRaw, rad);
    for (let i = 0; i < normBytes.length; i++) {
      const n = normBytes[i];
      const tempC = minC + (n / 255) * (maxC - minC);
      let r = lutBytes[n * 4];
      let g = lutBytes[n * 4 + 1];
      let b = lutBytes[n * 4 + 2];
      if (matchesIsotherm(pipeline.isotherm, tempC)) {
        [r, g, b] = pipeline.isotherm.color;
      }
      imgData.data[i * 4] = r;
      imgData.data[i * 4 + 1] = g;
      imgData.data[i * 4 + 2] = b;
      imgData.data[i * 4 + 3] = 255;
    }
    octx.putImageData(imgData, 0, 0);
    ctx.imageSmoothingEnabled = pipeline.upscaler !== "nearest";
    ctx.imageSmoothingQuality = "high";
    ctx.clearRect(0, 0, cv.width, cv.height);
    ctx.drawImage(off, 0, 0, cv.width, cv.height);
  }, [frame, compareFrame, splitRatio, lutBytes, minRaw, maxRaw, pipeline, rad, sensorW, sensorH]);

  const roiStatsMap = useMemo(() => {
    const map = new Map<string, ReturnType<typeof evaluateRoi>>();
    for (const r of rois) {
      map.set(r.id, evaluateRoi(r, frame.pixels, sensorW, sensorH, rad));
    }
    return map;
  }, [rois, frame, sensorW, sensorH, rad]);

  const toSensorCoords = useCallback(
    (e: React.MouseEvent<SVGSVGElement>): [number, number] => {
      const rect = e.currentTarget.getBoundingClientRect();
      const nx = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width));
      const ny = Math.max(0, Math.min(1, (e.clientY - rect.top) / rect.height));
      return [
        Math.min(sensorW - 1, Math.floor(nx * sensorW)),
        Math.min(sensorH - 1, Math.floor(ny * sensorH)),
      ];
    },
    [sensorW, sensorH],
  );

  const handleMouseMove = (e: React.MouseEvent<SVGSVGElement>): void => {
    const [sx, sy] = toSensorCoords(e);
    const raw = frame.pixels[sy * sensorW + sx];
    const info = { x: sx, y: sy, raw, tempC: rawToObjectC(raw, rad) };
    setHover(info);
    onHoverPixel?.(info);
    if (dragStart) setDragCur([sx, sy]);
  };

  const handleMouseDown = (e: React.MouseEvent<SVGSVGElement>): void => {
    const [sx, sy] = toSensorCoords(e);
    if (activeTool === "select") {
      onSelectRoi(null);
      return;
    }
    if (activeTool === "spot" || activeTool === "spot3") {
      onAddRoi({
        id: `spot_${Date.now()}`,
        label: `SP${rois.length + 1}`,
        geometry: {
          kind: "spot",
          x: sx,
          y: sy,
          kernel: activeTool === "spot3" ? 3 : 1,
          show_raw: false,
        },
        emissivity_override: null,
        reflected_temp_override_c: null,
        visible: true,
      });
      return;
    }
    if (activeTool === "polygon") {
      setPolyDraft((prev) => [...prev, [sx, sy]]);
      return;
    }
    setDragStart([sx, sy]);
    setDragCur([sx, sy]);
  };

  const handleDoubleClick = (): void => {
    if (activeTool === "polygon" && polyDraft.length >= 3) {
      onAddRoi({
        id: `poly_${Date.now()}`,
        label: `POLY${rois.length + 1}`,
        geometry: { kind: "polygon", points: polyDraft },
        emissivity_override: null,
        reflected_temp_override_c: null,
        visible: true,
      });
      setPolyDraft([]);
    }
  };

  const handleMouseUp = (): void => {
    if (!dragStart || !dragCur) return;
    const [x0, y0] = dragStart;
    const [x1, y1] = dragCur;
    const dist = Math.hypot(x1 - x0, y1 - y0);
    if (dist >= 3) {
      const id = `${activeTool}_${Date.now()}`;
      if (activeTool === "box") {
        onAddRoi({
          id,
          label: `BX${rois.length + 1}`,
          geometry: {
            kind: "box",
            x: Math.min(x0, x1),
            y: Math.min(y0, y1),
            w: Math.max(2, Math.abs(x1 - x0)),
            h: Math.max(2, Math.abs(y1 - y0)),
          },
          emissivity_override: null,
          reflected_temp_override_c: null,
          visible: true,
        });
      } else if (activeTool === "circle") {
        onAddRoi({
          id,
          label: `CR${rois.length + 1}`,
          geometry: { kind: "circle", cx: x0, cy: y0, radius: Math.max(3, Math.round(dist)) },
          emissivity_override: null,
          reflected_temp_override_c: null,
          visible: true,
        });
      } else if (activeTool === "line") {
        onAddRoi({
          id,
          label: `LN${rois.length + 1}`,
          geometry: { kind: "line", x0, y0, x1, y1 },
          emissivity_override: null,
          reflected_temp_override_c: null,
          visible: true,
        });
      } else if (activeTool === "delta") {
        onAddRoi({
          id,
          label: `ΔT${rois.length + 1}`,
          geometry: { kind: "delta", x0, y0, x1, y1 },
          emissivity_override: null,
          reflected_temp_override_c: null,
          visible: true,
        });
      }
    }
    setDragStart(null);
    setDragCur(null);
  };

  const loupeCenter = hover ?? {
    x: frame.max_x,
    y: frame.max_y,
    raw: frame.max_raw,
    tempC: rawToObjectC(frame.max_raw, rad),
  };

  return (
    <div className="relative flex flex-1 items-center justify-center overflow-hidden rounded-[10px] border border-[var(--tc-border)] bg-[var(--tc-canvas-bg)]">
      {/* Top-left telemetry pill */}
      <div className="pointer-events-none absolute top-2.5 left-2.5 z-10 flex items-center gap-2 rounded-[6px] border border-white/12 bg-black/72 px-2.5 py-1 font-mono-tech text-[11px] text-[#ededed] backdrop-blur-xs">
        <span className="inline-block h-1.5 w-1.5 rounded-full bg-[var(--tc-pass)]" />
        <span>
          {sensorW}×{sensorH}
        </span>
        <span className="text-white/35">·</span>
        <span>{backendMode}</span>
        <span className="text-white/35">·</span>
        <span>{pipeline.upscaler.replace("_", " ")}</span>
        {pipeline.sharpen_amount > 0 ? (
          <>
            <span className="text-white/35">·</span>
            <span>USM {Math.round(pipeline.sharpen_amount * 100)}%</span>
          </>
        ) : null}
        {pipeline.histogram_eq ? (
          <>
            <span className="text-white/35">·</span>
            <span className="text-[var(--tc-accent)]">HEQ</span>
          </>
        ) : null}
        <span className="text-white/35">·</span>
        <span>ε={rad.emissivity.toFixed(2)}</span>
      </div>

      {/* Main 4:3 thermal stage */}
      <div
        className="relative aspect-[4/3] max-h-full max-w-full transition-transform duration-150"
        style={{
          width: "min(100%, calc((100vh - 210px) * 1.3333))",
          transform: `scale(${zoom})`,
        }}
      >
        <canvas
          ref={canvasRef}
          data-thermal-canvas="true"
          width={960}
          height={720}
          className="block h-full w-full rounded-[4px]"
        />

        <svg
          viewBox={`0 0 ${sensorW} ${sensorH}`}
          className="absolute inset-0 h-full w-full select-none"
          style={{ cursor: activeTool === "select" ? "crosshair" : "cell" }}
          onMouseMove={handleMouseMove}
          onMouseLeave={() => {
            setHover(null);
            onHoverPixel?.(null);
          }}
          onMouseDown={handleMouseDown}
          onMouseUp={handleMouseUp}
          onDoubleClick={handleDoubleClick}
        >
          {compareFrame ? (
            <line
              x1={sensorW * splitRatio}
              y1={0}
              x2={sensorW * splitRatio}
              y2={sensorH}
              stroke="#ffffff"
              strokeWidth={0.6}
              strokeDasharray="2 1.5"
            />
          ) : null}

          {showGridOverlay
            ? Array.from({ length: gridRows * gridCols }, (_, idx) => {
                const r = Math.floor(idx / gridCols);
                const c = idx % gridCols;
                const gx = Math.floor(((c * 2 + 1) * sensorW) / (gridCols * 2));
                const gy = Math.floor(((r * 2 + 1) * sensorH) / (gridRows * 2));
                const tc = rawToObjectC(frame.pixels[gy * sensorW + gx], rad);
                return { key: `gp_${gx}_${gy}`, gx, gy, tc };
              }).map((pt) => (
                <g key={pt.key}>
                  <circle
                    cx={pt.gx}
                    cy={pt.gy}
                    r={1.1}
                    fill="#ffffff"
                    stroke="#000"
                    strokeWidth={0.3}
                  />
                  <text
                    x={pt.gx + 2}
                    y={pt.gy + 1.4}
                    fill="#ffffff"
                    stroke="#000000"
                    strokeWidth={0.45}
                    paintOrder="stroke"
                    fontSize={3.6}
                    fontFamily="var(--font-mono)"
                  >
                    {formatTemp(pt.tc, unit, 1)}
                  </text>
                </g>
              ))
            : null}

          {showHotCold ? (
            <g className="pointer-events-none">
              <g transform={`translate(${frame.max_x}, ${frame.max_y})`}>
                <path d="M-3,0 H3 M0,-3 V3" stroke="#ff5733" strokeWidth={0.65} />
                <polygon points="0,-5 -2,-2.2 2,-2.2" fill="#ff5733" />
                <text
                  x={3.6}
                  y={-2}
                  fill="#ffd2c4"
                  stroke="#000"
                  strokeWidth={0.65}
                  paintOrder="stroke"
                  fontSize={3.8}
                  fontFamily="var(--font-mono)"
                >
                  MAX {formatTemp(rawToObjectC(frame.max_raw, rad), unit, 1)}
                </text>
              </g>
              <g transform={`translate(${frame.min_x}, ${frame.min_y})`}>
                <path d="M-3,0 H3 M0,-3 V3" stroke="#5da3e5" strokeWidth={0.65} />
                <polygon points="0,5 -2,2.2 2,2.2" fill="#5da3e5" />
                <text
                  x={3.6}
                  y={4.5}
                  fill="#cde6ff"
                  stroke="#000"
                  strokeWidth={0.65}
                  paintOrder="stroke"
                  fontSize={3.8}
                  fontFamily="var(--font-mono)"
                >
                  MIN {formatTemp(rawToObjectC(frame.min_raw, rad), unit, 1)}
                </text>
              </g>
            </g>
          ) : null}

          {rois
            .filter((r) => r.visible)
            .map((roi) => {
              const st = roiStatsMap.get(roi.id);
              const isSelected = roi.id === selectedRoiId;
              const stroke = isSelected ? "#f7b23b" : "#ffffff";
              const g = roi.geometry;
              if (g.kind === "spot") {
                const label = st
                  ? `${roi.label} ${formatTemp(st.mean_c, unit, 1)}${g.show_raw ? ` [${Math.round(st.mean_raw)}]` : ""}`
                  : roi.label;
                return (
                  <g
                    key={roi.id}
                    onClick={(e) => {
                      e.stopPropagation();
                      onSelectRoi(roi.id);
                    }}
                  >
                    <circle
                      cx={g.x}
                      cy={g.y}
                      r={g.kernel >= 3 ? 2.6 : 1.6}
                      fill="none"
                      stroke={stroke}
                      strokeWidth={0.65}
                    />
                    <path
                      d={`M${g.x - 4},${g.y} H${g.x - 1.4} M${g.x + 1.4},${g.y} H${g.x + 4} M${g.x},${g.y - 4} V${g.y - 1.4} M${g.x},${g.y + 1.4} V${g.y + 4}`}
                      stroke={stroke}
                      strokeWidth={0.55}
                    />
                    <text
                      x={g.x + 4.8}
                      y={g.y + 5.2}
                      fill={stroke}
                      stroke="#000000"
                      strokeWidth={0.75}
                      paintOrder="stroke"
                      fontSize={4}
                      fontFamily="var(--font-mono)"
                    >
                      {label}
                    </text>
                  </g>
                );
              }
              if (g.kind === "box") {
                return (
                  <g
                    key={roi.id}
                    onClick={(e) => {
                      e.stopPropagation();
                      onSelectRoi(roi.id);
                    }}
                  >
                    <rect
                      x={g.x}
                      y={g.y}
                      width={g.w}
                      height={g.h}
                      fill="rgb(93 163 229 / 10%)"
                      stroke={isSelected ? "#f7b23b" : "#5da3e5"}
                      strokeWidth={0.7}
                    />
                    <text
                      x={g.x}
                      y={Math.max(4, g.y - 1.8)}
                      fill="#edf4fb"
                      stroke="#000"
                      strokeWidth={0.75}
                      paintOrder="stroke"
                      fontSize={3.9}
                      fontFamily="var(--font-mono)"
                    >
                      {roi.label} {st ? `${formatTemp(st.max_c, unit, 1)} max` : ""}
                    </text>
                  </g>
                );
              }
              if (g.kind === "circle") {
                return (
                  <g
                    key={roi.id}
                    onClick={(e) => {
                      e.stopPropagation();
                      onSelectRoi(roi.id);
                    }}
                  >
                    <circle
                      cx={g.cx}
                      cy={g.cy}
                      r={g.radius}
                      fill="rgb(111 211 159 / 10%)"
                      stroke={isSelected ? "#f7b23b" : "#6fd39f"}
                      strokeWidth={0.7}
                    />
                    <text
                      x={g.cx - g.radius}
                      y={Math.max(4, g.cy - g.radius - 1.5)}
                      fill="#e6fff2"
                      stroke="#000"
                      strokeWidth={0.75}
                      paintOrder="stroke"
                      fontSize={3.9}
                      fontFamily="var(--font-mono)"
                    >
                      {roi.label} {st ? `${formatTemp(st.mean_c, unit, 1)} avg` : ""}
                    </text>
                  </g>
                );
              }
              if (g.kind === "polygon") {
                const pts = g.points.map(([px, py]) => `${px},${py}`).join(" ");
                return (
                  <g
                    key={roi.id}
                    onClick={(e) => {
                      e.stopPropagation();
                      onSelectRoi(roi.id);
                    }}
                  >
                    <polygon
                      points={pts}
                      fill="rgb(111 211 159 / 12%)"
                      stroke={isSelected ? "#f7b23b" : "#6fd39f"}
                      strokeWidth={0.7}
                    />
                  </g>
                );
              }
              if (g.kind === "line" || g.kind === "delta") {
                const isDelta = g.kind === "delta";
                return (
                  <g
                    key={roi.id}
                    onClick={(e) => {
                      e.stopPropagation();
                      onSelectRoi(roi.id);
                    }}
                  >
                    <line
                      x1={g.x0}
                      y1={g.y0}
                      x2={g.x1}
                      y2={g.y1}
                      stroke={isSelected ? "#ffffff" : "#f7b23b"}
                      strokeWidth={0.75}
                      strokeDasharray={isDelta ? "2 1.2" : undefined}
                    />
                    <circle cx={g.x0} cy={g.y0} r={1.3} fill="#f7b23b" />
                    <circle cx={g.x1} cy={g.y1} r={1.3} fill="#f7b23b" />
                    <text
                      x={(g.x0 + g.x1) / 2}
                      y={Math.max(4, (g.y0 + g.y1) / 2 - 2)}
                      fill="#fde5b4"
                      stroke="#000"
                      strokeWidth={0.75}
                      paintOrder="stroke"
                      fontSize={3.9}
                      fontFamily="var(--font-mono)"
                    >
                      {roi.label}{" "}
                      {isDelta && st?.delta_c !== null && st?.delta_c !== undefined
                        ? `${st.delta_c >= 0 ? "+" : ""}${st.delta_c.toFixed(1)} ${unitSymbol(unit)}`
                        : st
                          ? `${formatTemp(st.max_c, unit, 1)} pk`
                          : ""}
                    </text>
                  </g>
                );
              }
              return null;
            })}

          {dragStart && dragCur ? (
            activeTool === "box" ? (
              <rect
                x={Math.min(dragStart[0], dragCur[0])}
                y={Math.min(dragStart[1], dragCur[1])}
                width={Math.abs(dragCur[0] - dragStart[0])}
                height={Math.abs(dragCur[1] - dragStart[1])}
                fill="rgb(247 178 59 / 12%)"
                stroke="#f7b23b"
                strokeWidth={0.7}
                strokeDasharray="2 1"
              />
            ) : activeTool === "circle" ? (
              <circle
                cx={dragStart[0]}
                cy={dragStart[1]}
                r={Math.hypot(dragCur[0] - dragStart[0], dragCur[1] - dragStart[1])}
                fill="rgb(247 178 59 / 12%)"
                stroke="#f7b23b"
                strokeWidth={0.7}
                strokeDasharray="2 1"
              />
            ) : (
              <line
                x1={dragStart[0]}
                y1={dragStart[1]}
                x2={dragCur[0]}
                y2={dragCur[1]}
                stroke="#f7b23b"
                strokeWidth={0.75}
                strokeDasharray="2 1"
              />
            )
          ) : null}

          {polyDraft.length > 0 ? (
            <polyline
              points={polyDraft.map(([px, py]) => `${px},${py}`).join(" ")}
              fill="none"
              stroke="#6fd39f"
              strokeWidth={0.75}
              strokeDasharray="1.5 1"
            />
          ) : null}
        </svg>
      </div>

      {/* Zoomed 7×7 Pixel-Grid Loupe */}
      {showLoupe ? (
        <div className="pointer-events-none absolute right-2.5 bottom-2.5 z-10 w-[166px] rounded-[8px] border border-white/15 bg-black/82 p-2 text-[#ededed] backdrop-blur-xs">
          <div className="mb-1.5 flex items-center justify-between font-mono-tech text-[10.5px]">
            <span className="text-white/65">
              LOUPE ({loupeCenter.x},{loupeCenter.y})
            </span>
            <span className="font-semibold text-[var(--tc-thermal)]">
              {formatTemp(loupeCenter.tempC, unit, 2)}
            </span>
          </div>
          <div className="grid grid-cols-7 gap-[1px] bg-white/15 p-[1px]">
            {LOUPE_CELLS.map((cell) => {
              const px = Math.max(0, Math.min(sensorW - 1, loupeCenter.x + cell.dx));
              const py = Math.max(0, Math.min(sensorH - 1, loupeCenter.y + cell.dy));
              const raw = frame.pixels[py * sensorW + px];
              const norm = Math.max(0, Math.min(1, (raw - minRaw) / Math.max(1, maxRaw - minRaw)));
              const [r, g, b] = samplePalette(palette, norm);
              return (
                <div
                  key={cell.key}
                  className="h-[19px] w-[19px]"
                  style={{
                    backgroundColor: `rgb(${r},${g},${b})`,
                    outline: cell.isCenter ? "1.5px solid #ffffff" : undefined,
                    zIndex: cell.isCenter ? 2 : 1,
                  }}
                />
              );
            })}
          </div>
          <div className="mt-1 flex items-center justify-between font-mono-tech text-[10px] text-white/60">
            <span>RAW {loupeCenter.raw}</span>
            <span>7×7 sensor grid</span>
          </div>
        </div>
      ) : null}
    </div>
  );
}
