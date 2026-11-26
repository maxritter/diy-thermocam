import {
  AlertTriangle,
  Download,
  FileQuestion,
  FolderOpen,
  HardDrive,
  LayoutGrid,
  List,
  Monitor,
  RefreshCw,
  Search,
  Trash2,
  Usb,
  X,
} from "lucide-react";
import { type MouseEvent, useCallback, useEffect, useMemo, useRef, useState } from "react";
import type { CameraState } from "@/app/CameraChip";
import { isMacPlatform, isTypingTarget } from "@/app/shortcuts";
import type { CaptureItem, CardInfo, capturesApi } from "@/lib/api/captures";
import { folderLabel } from "@/lib/settings";
import { formatTemp, rawToObjectC } from "@/lib/thermal";
import type { Material, TempUnit, ThermalFrame } from "@/lib/types";
import { CaptureCard } from "./CaptureCard";
import { currentPlatform, DeleteDialog } from "./DeleteDialog";
import { DEFAULT_MATERIAL } from "./edits";
import {
  cardText,
  type DayGroup,
  formatSize,
  groupByDay,
  parseCaptureName,
  systemLocale,
} from "./names";
import { createTaskQueue } from "./pictureQueue";
import {
  clearSelection,
  emptySelection,
  retainSelection,
  type Selection,
  selectAll,
  selectClick,
} from "./Selection";

export type CapturesApi = typeof capturesApi;
export type Tab = "camera" | "computer";
export type SortOrder = "newest" | "oldest" | "name";

const ICON = { size: 16, strokeWidth: 1.75, "aria-hidden": true } as const;
const SM = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

type Listing =
  | { status: "loading" }
  | { status: "none" }
  | { status: "error"; detail: string }
  | { status: "ready"; items: CaptureItem[]; card: CardInfo | null };

/** One capture as the library shows it. */
export interface LibraryEntry {
  item: CaptureItem;
  title: string;
  meta: string;
  taken: Date | null;
  other: boolean;
}

export interface LibraryProps {
  api: CapturesApi;
  camera: { state: CameraState; connectionId: string | null };
  capturesDir: string | null;
  /** Card pictures are drawn in this palette. */
  paletteId: string;
  tab: Tab;
  onTabChange: (tab: Tab) => void;
  /** Opens a capture; `entries` is the list in the order shown, for Previous and Next. */
  onOpen: (item: CaptureItem, entries: LibraryEntry[]) => void;
  notify: (msg: string) => void;
  locale?: string;
  now?: () => Date;
  /** Bumped by the page to reload after a capture's edits changed its name. */
  reloadKey?: number;
  unit?: TempUnit;
}

/** A capture's telling point: raw value, hot or cold, and its calibration. */
interface Telling {
  raw: number;
  hot: boolean;
  cal_offset: number;
  cal_slope: number;
}

/**
 * The temperature a card shows (captures.html#library): the hottest point, or the coldest when
 * the scene is cold, that is when the coldest lies further from the average than the hottest.
 */
export function tellingPoint(frame: ThermalFrame): { raw: number; hot: boolean } {
  let sum = 0;
  for (let i = 0; i < frame.pixels.length; i++) sum += frame.pixels[i];
  const mean = sum / Math.max(1, frame.pixels.length);
  const hot = frame.max_raw - mean >= mean - frame.min_raw;
  return { raw: hot ? frame.max_raw : frame.min_raw, hot };
}

/** "27.1 GB free of 29.8 GB", as the operating system counts gigabytes. */
function cardSpace(card: CardInfo): { text: string; used: number } {
  const gb = (kb: number) => (kb / 1_048_576).toFixed(1);
  const used = card.totalKb > 0 ? 1 - card.freeKb / card.totalKb : 0;
  return { text: `${gb(card.freeKb)} GB free of ${gb(card.totalKb)} GB`, used };
}

function detailFor(item: CaptureItem, frames: number | undefined): string {
  if (item.kind === "recording") {
    const n = frames ?? item.frames;
    return n ? `${n} frames` : "";
  }
  if (item.kind === "other") return item.size !== null ? formatSize(item.size) : "";
  return item.size !== null ? formatSize(item.size) : "";
}

function sortEntries(entries: LibraryEntry[], order: SortOrder): LibraryEntry[] {
  const time = (e: LibraryEntry) => e.taken?.getTime() ?? e.item.mtimeMs ?? 0;
  const out = [...entries];
  if (order === "name") out.sort((a, b) => a.title.localeCompare(b.title));
  else if (order === "oldest") out.sort((a, b) => time(a) - time(b));
  else out.sort((a, b) => time(b) - time(a));
  return out;
}

/** Newest first, whatever the sort: pictures fill in from the top of a newest-first list. */
function newestFirst(items: CaptureItem[]): CaptureItem[] {
  const time = (i: CaptureItem) => parseCaptureName(i.fileName).taken?.getTime() ?? i.mtimeMs ?? 0;
  return [...items].sort((a, b) => time(b) - time(a));
}

function StateCard({
  icon,
  title,
  body,
  action,
  danger,
}: {
  icon: React.ReactNode;
  title: string;
  body: string;
  action?: React.ReactNode;
  danger?: boolean;
}): React.JSX.Element {
  return (
    <div className="cp-state-wrap">
      <div className="cp-state" role="status">
        <span className={`ic${danger ? " danger" : ""}`}>{icon}</span>
        <strong>{title}</strong>
        <span>{body}</span>
        {action}
      </div>
    </div>
  );
}

const BIG = { size: 18, strokeWidth: 1.75, "aria-hidden": true } as const;

/**
 * The library (captures.html#library, #empty-states): On the camera and On this computer,
 * cards grouped by day, pictures filled in newest first, two at a time.
 */
export function Library({
  api,
  camera,
  capturesDir,
  paletteId,
  tab,
  onTabChange,
  onOpen,
  notify,
  locale = systemLocale(),
  now = () => new Date(),
  reloadKey = 0,
  unit = "celsius",
}: LibraryProps): React.JSX.Element {
  const [listings, setListings] = useState<Record<Tab, Listing>>({
    camera: { status: "loading" },
    computer: { status: "loading" },
  });
  const [pictures, setPictures] = useState<Map<string, string | null>>(() => new Map());
  const [names, setNames] = useState<Map<string, string>>(() => new Map());
  const [frames, setFrames] = useState<Map<string, number>>(() => new Map());
  const [computerNames, setComputerNames] = useState<Set<string>>(() => new Set());
  const [temps, setTemps] = useState<Map<string, Telling>>(() => new Map());
  const [materials, setMaterials] = useState<Map<string, Material>>(() => new Map());
  const [search, setSearch] = useState("");
  const [sort, setSort] = useState<SortOrder>("newest");
  const [view, setView] = useState<"grid" | "list">("grid");
  const [reload, setReload] = useState(0);
  const [selection, setSelection] = useState<Selection>(emptySelection);
  /** Captures the Delete dialog asks about: the selection or one card's quick Delete. */
  const [toDelete, setToDelete] = useState<LibraryEntry[] | null>(null);
  const [deleteFailed, setDeleteFailed] = useState<string | null>(null);

  const pictureQueue = useMemo(() => createTaskQueue(2), []);
  const nameQueue = useMemo(() => createTaskQueue(4), []);
  const gen = useRef(0);
  const live = useRef({ paletteId, notify });
  live.current = { paletteId, notify };

  const cameraOnline = camera.state === "camera" || camera.state === "demo";

  // Load the shown tab's list, then its typed names and pictures. A tab change, a reload or a
  // new camera drops whatever is still waiting.
  // biome-ignore lint/correctness/useExhaustiveDependencies: reload and reloadKey re-run on purpose.
  useEffect(() => {
    const myGen = ++gen.current;
    pictureQueue.clear();
    nameQueue.clear();
    const current = () => myGen === gen.current;
    const setListing = (l: Listing) => {
      if (!current()) return;
      // A reload keeps the cards on screen (and what is selected) until the new list arrives.
      setListings((prev) =>
        l.status === "loading" && prev[tab].status === "ready" ? prev : { ...prev, [tab]: l },
      );
    };

    const fill = (items: CaptureItem[]) => {
      nameQueue.add(
        items
          .filter((i) => i.kind !== "other")
          .map((item) => ({
            key: `name:${item.id}`,
            run: async () => {
              if (!current()) return;
              const { sidecar } = await api.sidecarLoad(item.ref);
              const typed = sidecar?.notes.name.trim();
              if (current() && sidecar) {
                setMaterials((prev) => new Map(prev).set(item.id, sidecar.material));
              }
              if (current()) {
                setNames((prev) => {
                  const next = new Map(prev);
                  if (typed) next.set(item.id, typed);
                  else next.delete(item.id);
                  return next;
                });
              }
            },
          })),
      );
      pictureQueue.add(
        newestFirst(items)
          .filter((i) => i.kind !== "other")
          .map((item) => ({
            key: `pic:${item.id}`,
            run: async () => {
              if (!current()) return;
              // A camera video's frame count comes from listing its folder, beside the picture.
              const counted =
                item.kind === "recording" && item.ref.kind === "camera" && item.frames === null
                  ? api
                      .cameraFolderFrames(item.ref.sd_path)
                      .then((n) => {
                        if (current()) setFrames((prev) => new Map(prev).set(item.id, n));
                      })
                      .catch(() => {})
                  : null;
              let url: string | null;
              try {
                url = (await api.thumbnail(item, live.current.paletteId)) || null;
              } catch {
                url = null;
              }
              await counted;
              if (current()) setPictures((prev) => new Map(prev).set(item.id, url));
              // The telling temperature needs the frame itself: a bounded read, after the
              // picture. A camera video's frames stay on the card until it is opened.
              if (url === null || (item.kind === "recording" && item.ref.kind === "camera")) {
                return;
              }
              try {
                const fr = await api.loadFirstFrame(item);
                if (!current()) return;
                const t = tellingPoint(fr);
                setTemps((prev) =>
                  new Map(prev).set(item.id, {
                    ...t,
                    cal_offset: fr.cal_offset,
                    cal_slope: fr.cal_slope,
                  }),
                );
              } catch {
                // No chip for a frame that can't be read; the card stays as it is.
              }
            },
          })),
      );
    };

    if (tab === "camera") {
      if (!cameraOnline) {
        setListing({ status: "none" });
        return;
      }
      setListing({ status: "loading" });
      void (async () => {
        try {
          const serial = await api.cameraSerial();
          if (!serial) {
            setListing({ status: "none" });
            return;
          }
          const { card, items } = await api.listCamera(serial);
          setListing({ status: "ready", items, card });
          if (!current()) return;
          fill(items);
          // "Copied" marks camera captures already on this computer.
          void api
            .listComputer()
            .then((local) => {
              if (current()) setComputerNames(new Set(local.map((i) => i.fileName.toLowerCase())));
            })
            .catch(() => {});
        } catch (err) {
          const detail =
            err && typeof err === "object" && "detail" in err
              ? String((err as { detail: unknown }).detail)
              : String(err);
          setListing({ status: "error", detail });
        }
      })();
    } else {
      setListing({ status: "loading" });
      void (async () => {
        try {
          const items = await api.listComputer();
          setListing({ status: "ready", items, card: null });
          if (current()) fill(items);
        } catch (err) {
          const detail =
            err && typeof err === "object" && "detail" in err
              ? String((err as { detail: unknown }).detail)
              : String(err);
          setListing({ status: "error", detail });
        }
      })();
    }
  }, [tab, cameraOnline, camera.connectionId, api, pictureQueue, nameQueue, reload, reloadKey]);

  // Leaving the page drops the queue.
  useEffect(
    () => () => {
      gen.current++;
      pictureQueue.clear();
      nameQueue.clear();
    },
    [pictureQueue, nameQueue],
  );

  const listing = listings[tab];
  const items = listing.status === "ready" ? listing.items : [];

  const entries = useMemo<LibraryEntry[]>(() => {
    const q = search.trim().toLowerCase();
    const list = items.map((item) => {
      const typed = names.get(item.id);
      const { title, meta } = cardText(
        item.fileName,
        typed,
        detailFor(item, frames.get(item.id)),
        locale,
      );
      return {
        item,
        title,
        meta,
        taken: parseCaptureName(item.fileName).taken,
        other: item.kind === "other",
      };
    });
    const filtered = q
      ? list.filter(
          (e) => e.title.toLowerCase().includes(q) || e.item.fileName.toLowerCase().includes(q),
        )
      : list;
    return sortEntries(filtered, sort);
  }, [items, names, frames, search, sort, locale]);

  const groups: DayGroup<LibraryEntry>[] = groupByDay(entries, now(), locale);

  const counts: Record<Tab, number | null> = {
    camera: listings.camera.status === "ready" ? listings.camera.items.length : null,
    computer: listings.computer.status === "ready" ? listings.computer.items.length : null,
  };

  const capturesShown = items.filter((i) => i.kind !== "other").length;
  const picturesDone = items.filter((i) => i.kind !== "other" && pictures.has(i.id)).length;
  const reading = tab === "camera" && listing.status === "ready" && picturesDone < capturesShown;

  const openFile = useCallback(async () => {
    try {
      const item = await api.openFile();
      if (!item) return;
      api.rememberOpened(item);
      onOpen(item, [
        {
          item,
          title: cardText(item.fileName, null, "", locale).title,
          meta: "",
          taken: parseCaptureName(item.fileName).taken,
          other: false,
        },
      ]);
    } catch (err) {
      const detail =
        err && typeof err === "object" && "detail" in err
          ? String((err as { detail: unknown }).detail)
          : String(err);
      live.current.notify(`Couldn’t open the file: ${detail}`);
    }
  }, [api, onOpen, locale]);

  // The order selection ranges follow: the cards as shown, captures only.
  const order = useMemo(() => entries.filter((e) => !e.other).map((e) => e.item.id), [entries]);
  const orderRef = useRef(order);
  orderRef.current = order;

  // biome-ignore lint/correctness/useExhaustiveDependencies: a new tab starts with nothing selected.
  useEffect(() => {
    setSelection(clearSelection());
    setDeleteFailed(null);
  }, [tab]);
  useEffect(() => {
    setSelection((s) => (s.ids.size ? retainSelection(s, order) : s));
  }, [order]);

  const selected = entries.filter((e) => selection.ids.has(e.item.id));
  const selectedRef = useRef(selected);
  selectedRef.current = selected;

  const mac = isMacPlatform();
  const activate = (entry: LibraryEntry, e: MouseEvent) => {
    if (entry.other) return;
    const toggle = mac ? e.metaKey : e.ctrlKey;
    if (toggle || e.shiftKey) {
      setSelection((s) => selectClick(s, order, entry.item.id, { toggle, range: e.shiftKey }));
      return;
    }
    onOpen(entry.item, entries);
  };

  // ⌘A selects every capture shown, Esc clears, Delete asks about the selected ones.
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.isComposing || isTypingTarget(e.target)) return;
      if (document.querySelector("[role=dialog], [role=alertdialog], [role=menu]")) return;
      const mod = mac ? e.metaKey : e.ctrlKey;
      if (mod && !e.shiftKey && e.key.toLowerCase() === "a") {
        e.preventDefault();
        setSelection(selectAll(orderRef.current));
      } else if (e.key === "Escape" && selectedRef.current.length > 0) {
        e.preventDefault();
        setSelection(clearSelection());
      } else if ((e.key === "Delete" || e.key === "Backspace") && selectedRef.current.length) {
        e.preventDefault();
        setToDelete(selectedRef.current);
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [mac]);

  const confirmDelete = async () => {
    const list = toDelete ?? [];
    const failed: { id: string; title: string; detail: string }[] = [];
    if (tab === "camera") {
      const byPath = new Map(
        list.flatMap((e) => (e.item.ref.kind === "camera" ? [[e.item.ref.sd_path, e]] : [])),
      );
      const res = await api.deleteFromCamera([...byPath.keys()]);
      for (const f of res.failed) {
        const e = byPath.get(f.path);
        if (e) failed.push({ id: e.item.id, title: e.title, detail: f.detail });
      }
    } else {
      const byPath = new Map(
        list.flatMap((e) => (e.item.ref.kind === "local" ? [[e.item.ref.path, e]] : [])),
      );
      try {
        const res = await api.trash([...byPath.keys()]);
        for (const f of res.failed) {
          const e = byPath.get(f.path);
          if (e) failed.push({ id: e.item.id, title: e.title, detail: f.detail });
        }
      } catch (err) {
        const detail = String((err as { detail?: unknown })?.detail ?? err);
        for (const e of list) failed.push({ id: e.item.id, title: e.title, detail });
      }
    }
    setToDelete(null);
    // What could not be deleted stays selected, and the message names it.
    setSelection({
      ids: new Set(failed.map((f) => f.id)),
      anchor: null,
      base: new Set(failed.map((f) => f.id)),
    });
    setDeleteFailed(
      failed.length === 0
        ? null
        : `Couldn’t delete ${failed.map((f) => f.title).join(", ")}: ${failed[0].detail}`,
    );
    const gone = list.length - failed.length;
    if (gone > 0) {
      live.current.notify(
        tab === "camera"
          ? `Deleted ${gone} ${gone === 1 ? "capture" : "captures"} from the camera`
          : `Moved ${gone} ${gone === 1 ? "capture" : "captures"} to the ${
              currentPlatform() === "windows" ? "Recycle Bin" : "Trash"
            }`,
      );
    }
    setReload((n) => n + 1);
  };

  const reveal = (item: CaptureItem) => {
    if (item.ref.kind !== "local") return;
    void api.revealInFolder(item.ref.path).catch(() => {});
  };

  const copy = (item: CaptureItem) => {
    void api
      .copyToComputer(item)
      .then(() => {
        setComputerNames((prev) => new Set(prev).add(item.fileName.toLowerCase()));
        live.current.notify("Copied to this computer");
      })
      .catch((err) => live.current.notify(`Couldn’t copy: ${String(err?.detail ?? err)}`));
  };

  const space = listing.status === "ready" && listing.card ? cardSpace(listing.card) : null;

  /** The chip on a card, read with the capture's own material (or Live's starting one). */
  const chipFor = (id: string): { text: string; hot: boolean } | null => {
    const t = temps.get(id);
    if (!t) return null;
    const m = materials.get(id) ?? DEFAULT_MATERIAL;
    const c = rawToObjectC(t.raw, {
      cal_offset: t.cal_offset,
      cal_slope: t.cal_slope,
      emissivity: m.emissivity,
      reflected_temp_c: m.reflected_c,
      sensor_material: null,
    });
    return { text: formatTemp(c, unit), hot: t.hot };
  };

  let body: React.ReactNode;
  if (listing.status === "none") {
    body = (
      <StateCard
        icon={<Usb {...BIG} />}
        title="Camera not connected"
        body="Connect it with USB to see what is on its SD card."
      />
    );
  } else if (listing.status === "error") {
    body =
      tab === "camera" ? (
        <StateCard
          danger
          icon={<AlertTriangle {...BIG} />}
          title="Couldn’t read the SD card"
          body="The camera stopped answering while the list loaded."
          action={
            <button className="tc-btn" type="button" onClick={() => setReload((n) => n + 1)}>
              <RefreshCw {...ICON} />
              Try again
            </button>
          }
        />
      ) : (
        <StateCard
          danger
          icon={<AlertTriangle {...BIG} />}
          title="Couldn’t read the Captures folder"
          body={listing.detail}
          action={
            <button className="tc-btn" type="button" onClick={() => setReload((n) => n + 1)}>
              <RefreshCw {...ICON} />
              Try again
            </button>
          }
        />
      );
  } else if (listing.status === "loading") {
    body = (
      <>
        <div className="cp-day">
          {tab === "camera" ? "Reading the card" : "Reading the Captures folder"}
        </div>
        <div className="cp-grid" aria-busy="true">
          {[0, 1, 2, 3, 4].map((i) => (
            <div key={i} className="cp-cap">
              <div className="cp-skel" style={{ aspectRatio: "4 / 3" }} />
            </div>
          ))}
        </div>
      </>
    );
  } else if (tab === "camera" && listing.card && !listing.card.present) {
    body = (
      <StateCard
        icon={<HardDrive {...BIG} />}
        title="No SD card in the camera"
        body="Put a microSD card in the camera to see its captures here."
      />
    );
  } else if (items.length === 0) {
    body =
      tab === "camera" ? (
        <StateCard
          icon={<HardDrive {...BIG} />}
          title="Nothing on the SD card yet"
          body="Pictures and videos you take with the camera appear here."
        />
      ) : (
        <StateCard
          icon={<Monitor {...BIG} />}
          title="Nothing on this computer yet"
          body="Snapshots and recordings from Live land here, and files you open."
          action={
            <button className="tc-btn" type="button" onClick={() => void openFile()}>
              <FolderOpen {...ICON} />
              Open file…
            </button>
          }
        />
      );
  } else if (entries.length === 0) {
    body = (
      <StateCard
        icon={<Search {...BIG} />}
        title="No captures match"
        body={`Nothing here is called “${search.trim()}”.`}
      />
    );
  } else if (view === "list") {
    body = (
      <table className="cp-list">
        <thead>
          <tr>
            <th aria-label="Picture" />
            <th>Name</th>
            <th>Kind</th>
            <th>Size</th>
            <th>File</th>
          </tr>
        </thead>
        <tbody>
          {entries.map((e) => {
            const pic = pictures.get(e.item.id);
            return (
              <tr key={e.item.id} className={selection.ids.has(e.item.id) ? "sel" : undefined}>
                <td>
                  <div className="pic">{pic ? <img src={pic} alt="" /> : null}</div>
                </td>
                <td>
                  <button
                    className="name-btn"
                    type="button"
                    disabled={e.other}
                    onClick={(ev) => activate(e, ev)}
                  >
                    {e.title}
                  </button>
                </td>
                <td>
                  {e.item.kind === "recording"
                    ? "Recording"
                    : e.item.kind === "snapshot"
                      ? "Picture"
                      : "Other file"}
                </td>
                <td>{detailFor(e.item, frames.get(e.item.id))}</td>
                <td className="mono">{e.item.fileName}</td>
              </tr>
            );
          })}
        </tbody>
      </table>
    );
  } else {
    body = groups.map((g, gi) => (
      <section key={g.key} aria-label={g.sub ? `${g.title}, ${g.sub}` : g.title}>
        <div className="cp-day">
          {g.title} {g.sub ? <span>{g.sub}</span> : null}
          {gi === 0 && reading ? (
            <span className="cp-day-note">
              Reading the card · {picturesDone} of {capturesShown} pictures
            </span>
          ) : null}
        </div>
        <div className="cp-grid">
          {g.items.map((e) =>
            e.other ? (
              <div key={e.item.id} className="cp-cap">
                <div className="th">
                  <div className="cp-nopic" role="img" aria-label="Not a capture">
                    <FileQuestion {...SM} />
                    Not a capture
                  </div>
                </div>
                <span className="cp-card-cap" style={{ cursor: "default" }}>
                  <span className="t">{e.title}</span>
                  <span className="m">{e.meta || " "}</span>
                </span>
              </div>
            ) : (
              <CaptureCard
                key={e.item.id}
                item={e.item}
                title={e.title}
                meta={e.meta || " "}
                picture={pictures.has(e.item.id) ? (pictures.get(e.item.id) ?? null) : undefined}
                selected={selection.ids.has(e.item.id)}
                copied={
                  e.item.where === "camera" && computerNames.has(e.item.fileName.toLowerCase())
                }
                chip={chipFor(e.item.id)}
                onActivate={(ev) => activate(e, ev)}
                onToggleSelect={(ev) =>
                  setSelection((s) =>
                    selectClick(s, order, e.item.id, {
                      toggle: !ev.shiftKey || (mac ? ev.metaKey : ev.ctrlKey),
                      range: ev.shiftKey,
                    }),
                  )
                }
                onCopy={
                  e.item.where === "camera" && e.item.kind === "snapshot"
                    ? () => copy(e.item)
                    : undefined
                }
                onReveal={e.item.where === "computer" ? () => reveal(e.item) : undefined}
                onDelete={() => setToDelete([e])}
              />
            ),
          )}
        </div>
      </section>
    ));
  }

  return (
    <div className="cp-lib">
      <div className="cp-lib-bar">
        <div className="left">
          <fieldset className="st-seg" aria-label="Where">
            <button
              type="button"
              aria-pressed={tab === "camera"}
              onClick={() => onTabChange("camera")}
            >
              <HardDrive {...SM} className="cp-seg-ic" />
              On the camera{counts.camera !== null ? ` · ${counts.camera}` : ""}
            </button>
            <button
              type="button"
              aria-pressed={tab === "computer"}
              onClick={() => onTabChange("computer")}
            >
              <Monitor {...SM} className="cp-seg-ic" />
              On this computer{counts.computer !== null ? ` · ${counts.computer}` : ""}
            </button>
          </fieldset>
          {tab === "camera" && space ? (
            <span className="cp-store">
              <span className="cp-meter" aria-hidden="true">
                <i style={{ width: `${Math.round(space.used * 100)}%` }} />
              </span>
              {space.text}
            </span>
          ) : tab === "computer" ? (
            <span className="cp-store">
              <FolderOpen {...SM} />
              {folderLabel(capturesDir)}
            </span>
          ) : null}
        </div>
        <div className="right">
          <span className="cp-search">
            <input
              className="st-input"
              placeholder="Search"
              aria-label="Search captures"
              value={search}
              onChange={(e) => setSearch(e.target.value)}
            />
            <Search {...SM} />
          </span>
          <select
            className="st-select"
            style={{ width: "auto" }}
            aria-label="Sort"
            value={sort}
            onChange={(e) => setSort(e.target.value as SortOrder)}
          >
            <option value="newest">Newest first</option>
            <option value="oldest">Oldest first</option>
            <option value="name">Name</option>
          </select>
          <fieldset className="st-seg" aria-label="View">
            <button
              type="button"
              aria-pressed={view === "grid"}
              aria-label="Grid"
              onClick={() => setView("grid")}
            >
              <LayoutGrid {...SM} style={{ verticalAlign: -3 }} />
            </button>
            <button
              type="button"
              aria-pressed={view === "list"}
              aria-label="List with details"
              onClick={() => setView("list")}
            >
              <List {...SM} style={{ verticalAlign: -3 }} />
            </button>
          </fieldset>
          <button className="tc-btn" type="button" onClick={() => void openFile()}>
            <FolderOpen {...ICON} />
            Open file…
          </button>
          <button
            className="st-icon-btn bordered"
            type="button"
            aria-label={tab === "camera" ? "Reload the list from the camera" : "Reload the list"}
            title={tab === "camera" ? "Reload the list from the camera" : "Reload the list"}
            onClick={() => setReload((n) => n + 1)}
          >
            <RefreshCw {...ICON} />
          </button>
        </div>
      </div>
      <div className={`cp-lib-body${selected.length ? " selecting" : ""}`}>
        {deleteFailed ? (
          <div className="cp-banner" role="alert">
            <AlertTriangle {...SM} className="ic" />
            <span className="grow">{deleteFailed}</span>
            <button
              className="st-icon-btn"
              type="button"
              aria-label="Dismiss"
              onClick={() => setDeleteFailed(null)}
            >
              <X {...SM} />
            </button>
          </div>
        ) : null}
        {body}
      </div>
      {selected.length > 0 && listing.status === "ready" ? (
        <div className="cp-selbar" role="toolbar" aria-label="Selected captures">
          <span className="n">{selected.length} selected</span>
          {tab === "camera" ? (
            <button
              className="tc-btn tc-btn-sm"
              type="button"
              disabled={!selected.some((e) => e.item.kind === "snapshot")}
              onClick={() => {
                for (const e of selected) if (e.item.kind === "snapshot") copy(e.item);
              }}
            >
              <Download {...SM} />
              Copy to computer
            </button>
          ) : null}
          <button
            className="tc-btn tc-btn-sm cp-danger-text"
            type="button"
            onClick={() => setToDelete(selected)}
          >
            <Trash2 {...SM} />
            Delete
          </button>
          <button
            className="st-icon-btn"
            type="button"
            aria-label="Clear selection"
            onClick={() => setSelection(clearSelection())}
          >
            <X {...SM} />
          </button>
        </div>
      ) : null}
      <DeleteDialog
        open={toDelete !== null}
        where={tab}
        names={(toDelete ?? []).map((e) => e.title)}
        onConfirm={confirmDelete}
        onCancel={() => setToDelete(null)}
      />
    </div>
  );
}
