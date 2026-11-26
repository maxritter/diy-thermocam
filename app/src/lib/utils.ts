/**
 * Raw bytes from a Rust command or channel as an ArrayBuffer of exactly those bytes. Tauri's IPC
 * channel gives an ArrayBuffer; its postMessage fallback gives a list of numbers.
 */
export function rawBytes(raw: ArrayBuffer | ArrayBufferView | number[]): ArrayBuffer {
  if (raw instanceof ArrayBuffer) return raw;
  if (ArrayBuffer.isView(raw)) {
    const copy = new Uint8Array(raw.byteLength);
    copy.set(new Uint8Array(raw.buffer, raw.byteOffset, raw.byteLength));
    return copy.buffer;
  }
  return Uint8Array.from(raw).buffer;
}

/** A command's rejection: `{kind, detail}` from Rust, or an Error or string from older calls. */
export interface CommandError {
  kind: string;
  detail: string;
}

export function commandError(err: unknown): CommandError {
  if (err && typeof err === "object" && "detail" in err) {
    const e = err as { kind?: unknown; detail?: unknown };
    return { kind: typeof e.kind === "string" ? e.kind : "other", detail: String(e.detail) };
  }
  if (err instanceof Error) return { kind: "other", detail: err.message };
  return { kind: "other", detail: String(err) };
}

/** The last name in a path, with either slash. */
export function baseName(path: string): string {
  const parts = path.split(/[\\/]+/).filter(Boolean);
  return parts[parts.length - 1] ?? path;
}
