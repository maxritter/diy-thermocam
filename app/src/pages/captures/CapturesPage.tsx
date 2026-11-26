import { useEffect, useState } from "react";
import type { CameraState } from "@/app/CameraChip";
import { type CaptureItem, capturesApi } from "@/lib/api/captures";
import type { Palette, TempUnit } from "@/lib/types";
import { type CapturesApi, Library, type LibraryEntry, type Tab } from "./Library";
import { cardText, parseCaptureName, systemLocale } from "./names";
import { OpenedCapture } from "./OpenedCapture";
import { RecordingPlayer } from "./RecordingPlayer";
import "./captures.css";

export interface CapturesPageProps {
  /** The Rust calls; tests pass a stub. */
  api?: CapturesApi;
  camera: { state: CameraState; connectionId: string | null };
  capturesDir: string | null;
  palettes: Palette[];
  /** The last palette used: card pictures and captures without edits start in it. */
  paletteId: string;
  recentPaletteIds: string[];
  onImportPalette?: (name: string, text: string) => Promise<void>;
  unit: TempUnit;
  onUnitChange?: (unit: TempUnit) => void;
  engineeringDetails: boolean;
  /** A short message at the bottom of the window. */
  notify: (msg: string) => void;
  locale?: string;
  now?: () => Date;
  /** Open this capture (Open file… from Live's first start); `at` makes each request new. */
  openRequest?: { item: CaptureItem; at: number } | null;
}

/**
 * Captures (captures.html): the library of the camera's card and this computer, and an opened
 * capture or recording in the Live viewer.
 */
export function CapturesPage(props: CapturesPageProps): React.JSX.Element {
  const api = props.api ?? capturesApi;
  const [tab, setTab] = useState<Tab>("camera");
  const [opened, setOpened] = useState<{ item: CaptureItem; entries: LibraryEntry[] } | null>(null);
  const [reloadKey, setReloadKey] = useState(0);

  const request = props.openRequest;
  const locale = props.locale ?? systemLocale();
  useEffect(() => {
    if (!request) return;
    const { item } = request;
    setTab(item.where);
    setOpened({
      item,
      entries: [
        {
          item,
          title: cardText(item.fileName, null, "", locale).title,
          meta: "",
          taken: parseCaptureName(item.fileName).taken,
          other: false,
        },
      ],
    });
  }, [request, locale]);

  if (opened) {
    const shared = {
      api,
      item: opened.item,
      entries: opened.entries,
      candidates: opened.entries,
      onBack: () => {
        setOpened(null);
        setReloadKey((n) => n + 1);
      },
      onOpen: (item: CaptureItem) => setOpened({ item, entries: opened.entries }),
      palettes: props.palettes,
      paletteId: props.paletteId,
      recentPaletteIds: props.recentPaletteIds,
      onImportPalette:
        props.onImportPalette ??
        (async () => props.notify("Palettes are imported in Live, under All palettes.")),
      unit: props.unit,
      onUnitChange: props.onUnitChange,
      engineeringDetails: props.engineeringDetails,
      notify: props.notify,
      locale: props.locale,
    };
    return opened.item.kind === "recording" ? (
      <RecordingPlayer key={opened.item.id} {...shared} />
    ) : (
      <OpenedCapture key={opened.item.id} {...shared} />
    );
  }

  return (
    <Library
      api={api}
      camera={props.camera}
      capturesDir={props.capturesDir}
      paletteId={props.paletteId}
      tab={tab}
      onTabChange={setTab}
      onOpen={(item, entries) => setOpened({ item, entries })}
      notify={props.notify}
      locale={props.locale}
      now={props.now}
      reloadKey={reloadKey}
      unit={props.unit}
    />
  );
}
