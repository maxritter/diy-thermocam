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
