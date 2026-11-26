/**
 * Undo and Redo for an opened capture's edits (captures.html#opened): up to `limit` steps back.
 * Consecutive changes to the same field (typing a name) can share one step via `mergeKey`.
 */
export interface History<T> {
  past: T[];
  present: T;
  future: T[];
  limit: number;
  /** The field the last step changed; a commit with the same key replaces that step. */
  mergeKey: string | null;
}

export const HISTORY_LIMIT = 100;

export function createHistory<T>(initial: T, limit = HISTORY_LIMIT): History<T> {
  return { past: [], present: initial, future: [], limit, mergeKey: null };
}

export function commit<T>(h: History<T>, next: T, mergeKey?: string): History<T> {
  if (Object.is(next, h.present)) return h;
  if (mergeKey && mergeKey === h.mergeKey) {
    return { ...h, present: next, future: [] };
  }
  const past = [...h.past, h.present];
  if (past.length > h.limit) past.splice(0, past.length - h.limit);
  return { ...h, past, present: next, future: [], mergeKey: mergeKey ?? null };
}

export function undo<T>(h: History<T>): History<T> {
  if (h.past.length === 0) return h;
  const past = h.past.slice(0, -1);
  const present = h.past[h.past.length - 1];
  return { ...h, past, present, future: [h.present, ...h.future], mergeKey: null };
}

export function redo<T>(h: History<T>): History<T> {
  if (h.future.length === 0) return h;
  const [present, ...future] = h.future;
  return { ...h, past: [...h.past, h.present], present, future, mergeKey: null };
}

export const canUndo = <T>(h: History<T>): boolean => h.past.length > 0;
export const canRedo = <T>(h: History<T>): boolean => h.future.length > 0;
