/**
 * Selecting captures in the library (captures.html#library): the checkbox or ⌘/Ctrl-click
 * adds or removes one; Shift-click selects the range from the capture clicked last (replacing
 * the previous range, or adding to what was selected with ⌘ held); ⌘A selects all shown.
 */
export interface Selection {
  ids: ReadonlySet<string>;
  /** The capture clicked last without Shift: where a range starts. */
  anchor: string | null;
  /** What was selected when the anchor was set; a range is added to it with ⌘ held. */
  base: ReadonlySet<string>;
}

export function emptySelection(): Selection {
  return { ids: new Set(), anchor: null, base: new Set() };
}

export const clearSelection = emptySelection;

/** One click on a capture. `order` is the list in the order shown. */
export function selectClick(
  sel: Selection,
  order: readonly string[],
  id: string,
  mods: { toggle: boolean; range: boolean },
): Selection {
  const from = sel.anchor === null ? -1 : order.indexOf(sel.anchor);
  const to = order.indexOf(id);
  if (mods.range && from >= 0 && to >= 0) {
    const ids = new Set(mods.toggle ? sel.base : []);
    const [lo, hi] = from < to ? [from, to] : [to, from];
    for (let i = lo; i <= hi; i++) ids.add(order[i]);
    return { ids, anchor: sel.anchor, base: sel.base };
  }
  const ids = new Set(sel.ids);
  if (ids.has(id)) ids.delete(id);
  else ids.add(id);
  return { ids, anchor: id, base: new Set(ids) };
}

export function selectAll(order: readonly string[]): Selection {
  const ids = new Set(order);
  return { ids, anchor: order[0] ?? null, base: new Set(ids) };
}

/** Keeps only what is still listed (after a delete, a reload or a search). */
export function retainSelection(sel: Selection, order: readonly string[]): Selection {
  const shown = new Set(order);
  const keep = (s: ReadonlySet<string>) => new Set([...s].filter((id) => shown.has(id)));
  return {
    ids: keep(sel.ids),
    anchor: sel.anchor !== null && shown.has(sel.anchor) ? sel.anchor : null,
    base: keep(sel.base),
  };
}
