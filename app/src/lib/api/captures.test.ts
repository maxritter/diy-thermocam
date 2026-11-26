import { describe, expect, test } from "bun:test";
import { capturesApi } from "./captures";

describe("a damaged edits file", () => {
  test("is still named when its capture opens after the library set it aside", async () => {
    const ref = { kind: "camera" as const, serial: "demo", sd_path: "/20261123180233.DAT" };
    // The library reads names first: the damaged file is set aside then.
    const listed = await capturesApi.sidecarLoad(ref);
    expect(listed.quarantined).toBe("20261123180233.thermo.json.bad");
    // Opening the capture still says so, also when an open is repeated (React's dev mode).
    const opened = await capturesApi.sidecarLoad(ref, true);
    expect(opened).toEqual({ sidecar: null, quarantined: "20261123180233.thermo.json.bad" });
    expect((await capturesApi.sidecarLoad(ref, true)).quarantined).toBe(opened.quarantined);
    // Dismissing the message ends it; the library never shows it.
    capturesApi.forgetSetAside(ref);
    expect((await capturesApi.sidecarLoad(ref, true)).quarantined).toBeNull();
    expect((await capturesApi.sidecarLoad(ref)).quarantined).toBeNull();
  });
});
