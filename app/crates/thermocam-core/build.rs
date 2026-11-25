use std::collections::BTreeMap;
use std::env;
use std::fmt::Write as _;
use std::fs;
use std::path::PathBuf;

fn pretty_label(key: &str) -> &'static str {
    match key {
        "arctic" => "Arctic",
        "blackHot" => "Black Hot",
        "blueRed" => "Blue Red",
        "coldest" => "Coldest",
        "contrast" => "Contrast",
        "doubleRainbow" => "Double Rainbow",
        "grayRed" => "Gray Red",
        "glowBow" => "Glowbow",
        "grayscale" => "Grayscale",
        "hottest" => "Hottest",
        "ironblack" => "Ironblack",
        "lava" => "Lava",
        "medical" => "Medical",
        "rainbow" => "Rainbow",
        "wheel1" => "Wheel 1",
        "wheel2" => "Wheel 2",
        "wheel3" => "Wheel 3",
        "whiteHot" => "White Hot",
        "yellow" => "Yellow",
        _ => "Custom",
    }
}

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR"));
    let fw_root = manifest_dir
        .join("../../..")
        .join("firmware")
        .join("3.0");

    let defines_path = fw_root.join("include/globaldefines.h");
    let schemes_path = fw_root.join("src/general/colorschemes.cpp");
    let thermal_path = fw_root.join("src/thermal/thermal.cpp");

    println!("cargo:rerun-if-changed={}", defines_path.display());
    println!("cargo:rerun-if-changed={}", schemes_path.display());
    println!("cargo:rerun-if-changed={}", thermal_path.display());

    let defines_src = fs::read_to_string(&defines_path)
        .unwrap_or_else(|e| panic!("failed to read {}: {e}", defines_path.display()));
    let schemes_src = fs::read_to_string(&schemes_path)
        .unwrap_or_else(|e| panic!("failed to read {}: {e}", schemes_path.display()));
    let thermal_src = fs::read_to_string(&thermal_path)
        .unwrap_or_else(|e| panic!("failed to read {}: {e}", thermal_path.display()));

    // 1. Parse `#define colorScheme_<name> <num>`
    let mut name_to_id: BTreeMap<String, u8> = BTreeMap::new();
    for line in defines_src.lines() {
        let trimmed = line.trim();
        if let Some(rest) = trimmed.strip_prefix("#define colorScheme_") {
            let mut parts = rest.split_whitespace();
            if let (Some(name), Some(num_str)) = (parts.next(), parts.next()) {
                if name == "Total" {
                    continue;
                }
                let id: u8 = num_str
                    .parse()
                    .unwrap_or_else(|e| panic!("invalid colorScheme id '{num_str}': {e}"));
                name_to_id.insert(name.to_string(), id);
            }
        }
    }
    assert_eq!(
        name_to_id.len(),
        19,
        "expected 19 firmware color schemes in globaldefines.h, found {}",
        name_to_id.len()
    );

    // 2. Parse `const byte colorMap_<name>[] = { ... };`
    let mut name_to_rgb: BTreeMap<String, Vec<[u8; 3]>> = BTreeMap::new();
    let mut cursor = schemes_src.as_str();
    while let Some(pos) = cursor.find("const byte colorMap_") {
        let after = &cursor[pos + "const byte colorMap_".len()..];
        let bracket = after.find("[]").expect("expected [] after colorMap name");
        let name = after[..bracket].trim().to_string();
        let brace_open = after.find('{').expect("expected { for colorMap array");
        let brace_close = after[brace_open + 1..]
            .find('}')
            .expect("expected } for colorMap array");
        let body = &after[brace_open + 1..brace_open + 1 + brace_close];
        let nums: Vec<u8> = body
            .split(',')
            .filter_map(|tok| {
                let t = tok.trim();
                if t.is_empty() {
                    None
                } else {
                    Some(
                        t.parse::<u8>()
                            .unwrap_or_else(|e| panic!("bad byte '{t}' in {name}: {e}")),
                    )
                }
            })
            .collect();
        let triples: Vec<[u8; 3]> = nums.as_chunks::<3>().0.to_vec();
        name_to_rgb.insert(name, triples);
        cursor = &after[brace_open + 1 + brace_close + 1..];
    }

    // 3. Parse `case colorScheme_<name>: ... colorElements = <N>;`
    let mut name_to_elements: BTreeMap<String, usize> = BTreeMap::new();
    let mut tcursor = thermal_src.as_str();
    while let Some(pos) = tcursor.find("case colorScheme_") {
        let after = &tcursor[pos + "case colorScheme_".len()..];
        let colon = after.find(':').expect("expected colon after case");
        let name = after[..colon].trim().to_string();
        let after_colon = &after[colon + 1..];
        let elem_pos = after_colon
            .find("colorElements = ")
            .expect("expected colorElements assignment");
        let after_eq = &after_colon[elem_pos + "colorElements = ".len()..];
        let semi = after_eq.find(';').expect("expected semicolon");
        let elements: usize = after_eq[..semi]
            .trim()
            .parse()
            .unwrap_or_else(|e| panic!("invalid colorElements for {name}: {e}"));
        name_to_elements.insert(name, elements);
        tcursor = &after_eq[semi + 1..];
    }

    // Order by numeric ID 0..=18
    let mut ordered: BTreeMap<u8, (String, usize, Vec<[u8; 3]>)> = BTreeMap::new();
    for (name, id) in name_to_id {
        let rgb = name_to_rgb
            .remove(&name)
            .unwrap_or_else(|| panic!("missing colorMap_{name} in colorschemes.cpp"));
        let elements = *name_to_elements
            .get(&name)
            .unwrap_or_else(|| panic!("missing colorElements for {name} in thermal.cpp"));
        assert!(
            rgb.len() >= elements,
            "colorMap_{name} has {} entries but thermal.cpp expects {elements}",
            rgb.len()
        );
        ordered.insert(id, (name, elements, rgb));
    }

    // Generate Rust source
    let mut out = String::with_capacity(64 * 1024);
    out.push_str("// @generated by build.rs from firmware/3.0/src/general/colorschemes.cpp\n\n");
    out.push_str("#[derive(Debug, Clone, Copy)]\n");
    out.push_str("pub struct FirmwarePaletteDef {\n");
    out.push_str("    pub id: u8,\n");
    out.push_str("    pub key: &'static str,\n");
    out.push_str("    pub label: &'static str,\n");
    out.push_str("    pub elements: usize,\n");
    out.push_str("    pub rgb: &'static [[u8; 3]],\n");
    out.push_str("}\n\n");

    for (id, (name, elements, rgb)) in &ordered {
        let _ = writeln!(
            out,
            "static FW_PALETTE_{id}_RGB: &[[u8; 3]] = &["
        );
        for entry in rgb.iter().take(*elements) {
            let _ = writeln!(out, "    [{}, {}, {}],", entry[0], entry[1], entry[2]);
        }
        out.push_str("];\n\n");
        let _ = name;
    }

    out.push_str("pub const FIRMWARE_PALETTES: &[FirmwarePaletteDef] = &[\n");
    for (id, (name, elements, _)) in &ordered {
        let label = pretty_label(name);
        let _ = writeln!(
            out,
            "    FirmwarePaletteDef {{ id: {id}, key: \"{name}\", label: \"{label}\", elements: {elements}, rgb: FW_PALETTE_{id}_RGB }},"
        );
    }
    out.push_str("];\n");

    let out_dir = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR"));
    fs::write(out_dir.join("firmware_palettes.rs"), out).expect("write firmware_palettes.rs");
}
