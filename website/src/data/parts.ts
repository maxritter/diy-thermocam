/* Shops and prices for the /docs/partlist/ configurator.
   Every price was read on the shop's own page on 26 November 2026, logged out and without VAT:
   `us` is the US store's price in $, `eu` the European store's price in €.
   `euNo` means the European store doesn't sell the part to private buyers.
   Mouser's short product links break on eu.mouser.com, so Mouser links use its search, which lands on the
   product page in every region. Recheck prices when a part or shop changes. */

export type Region = 'us' | 'eu';
export type Dist = 'digikey' | 'mouser';
export type Shell = 'print' | 'laser';

export interface Offer {
  s: string;
  /** This offer's own product name and warning, when it differs from the part's. */
  n?: string;
  warn?: string;
  dist?: Dist;
  alt?: string;
  d?: string;
  us?: number;
  eu?: number;
  euNo?: string;
  u: Record<Region, string>;
  pending?: boolean;
}

export interface Part {
  id: string;
  name: string;
  d: string;
  own?: string;
  sensor?: '35' | '31';
  shell?: Shell[];
  pack?: boolean;
  need?: number;
  needLaser?: number;
  img?: string;
  fit?: boolean;
  icon?: 'header' | 'nut' | 'screw';
  warn?: string;
  euNote?: string;
  usNote?: string;
  anyIn?: Region[];
  anyD?: string;
  shops: Offer[];
}

const mouser = (q: string) => ({
  us: `https://www.mouser.com/c/?q=${encodeURIComponent(q)}`,
  eu: `https://eu.mouser.com/c/?q=${encodeURIComponent(q)}`,
});
const digikey = (path: string) => ({
  us: `https://www.digikey.com/en/products/detail/${path}`,
  eu: `https://www.digikey.de/de/products/detail/${path}`,
});
const same = (u: string) => ({ us: u, eu: u });
const ettinger = (slug: string, art: string) => same(`https://www.ettinger.de/p/${slug}/${art}`);

export const USD_PER_EUR = 1.08;

export const PARTS: Part[] = [
  {
    id: 'teensy',
    own: 'teensy',
    name: 'Teensy 4.1 with headers',
    d: 'SparkFun DEV-16996, pins already soldered',
    img: '/images/parts/s-teensy.jpg',
    fit: true,
    shops: [
      {
        s: 'DigiKey',
        dist: 'digikey',
        us: 35.95,
        eu: 31.91,
        u: digikey('sparkfun-electronics/16996/13158152'),
      },
      {
        s: 'Mouser',
        dist: 'mouser',
        us: 35.95,
        euNo: "Mouser doesn't sell it in Europe",
        u: mouser('DEV-16996'),
      },
    ],
  },
  {
    id: 'lepton35',
    own: 'lepton',
    sensor: '35',
    name: 'FLIR Lepton 3.5',
    d: '500-0771-01 · radiometric, 57°',
    img: '/images/parts/s-lepton35.jpg',
    shops: [
      {
        s: 'DigiKey',
        dist: 'digikey',
        us: 172,
        eu: 152.68,
        u: digikey('flir-lepton/500-0771-01/7606616'),
      },
      { s: 'Mouser', dist: 'mouser', us: 172, eu: 152.74, u: mouser('500-0771-01') },
      { s: 'GroupGets', us: 172, u: same('https://groupgets.com/products/flir-lepton-3-5') },
    ],
  },
  {
    id: 'lepton31',
    own: 'lepton',
    sensor: '31',
    name: 'FLIR Lepton 3.1R',
    d: '500-0758-03 · radiometric, 95°',
    img: '/images/parts/s-lepton31.jpg',
    euNote: 'Out of stock in Europe at both. DigiKey expects stock in March 2027.',
    shops: [
      {
        s: 'DigiKey',
        dist: 'digikey',
        us: 149,
        eu: 132.27,
        u: digikey('flir-lepton/500-0758-03/16694505'),
      },
      { s: 'Mouser', dist: 'mouser', us: 149, eu: 132.31, u: mouser('500-0758-03') },
    ],
  },
  {
    id: 'display',
    own: 'display',
    name: '3.2" touch display',
    d: 'ER-TFTM032-3 · pick 4-wire SPI, 5 V, resistive touch, pin header, no font chip',
    img: '/images/parts/s-display.jpg',
    shops: [
      {
        s: 'BuyDisplay',
        us: 13.51,
        u: same(
          'https://www.buydisplay.com/240x320-touch-screen-3-2-inch-tft-lcd-module-display-arduino-library',
        ),
      },
    ],
  },
  {
    id: 'lipo',
    name: 'LiPo, Adafruit 258, 1200 mAh',
    d: 'Protected, 5 × 34 × 62 mm, JST-PH plug. About 2.5–3.5 hours of use. Two printed battery spacers hold it in the middle of the cradle.',
    img: '/images/parts/s-lipo.jpg',
    shops: [
      {
        s: 'Adafruit',
        us: 9.95,
        u: same('https://www.adafruit.com/product/258'),
      },
      {
        s: 'DigiKey',
        dist: 'digikey',
        us: 9.95,
        u: digikey('adafruit-industries-llc/258/5054544'),
      },
      {
        s: 'ampul.eu',
        alt: 'More runtime: 505060, 2000 mAh',
        n: 'LiPo 505060, 2000 mAh',
        d: 'Protected, 5.0 × 50 × 60 mm, at most 5.0 mm thick. About 4–6 hours of use. Fits the cradle without the battery spacers.',
        warn: "ampul.eu doesn't list the plug, lead length or polarity: confirm a JST-PH plug, at least 50 mm of lead, and its + and − against the board before you order.",
        eu: 7.29,
        u: same('https://ampul.eu/en/li-po-batteries/4394-li-pol-battery-2000mah-37v-505060'),
      },
    ],
  },
  {
    id: 'sd',
    own: 'sd',
    name: 'microSD card, 16–32 GB',
    d: 'Any Class 10 card from any shop. Most builders already have one.',
    img: '/images/parts/s-microsd.jpg',
    anyIn: ['us', 'eu'],
    shops: [
      {
        s: 'SparkFun',
        us: 14.95,
        u: same('https://www.sparkfun.com/microsd-card-with-adapter-32gb-class-10.html'),
      },
    ],
  },
  {
    id: 'cable',
    own: 'cable',
    name: 'USB-C cable, 1 m',
    d: 'Adafruit 4474, USB-A to USB-C',
    img: '/images/parts/s-cable.jpg',
    shops: [
      {
        s: 'DigiKey',
        dist: 'digikey',
        us: 4.95,
        eu: 4.39,
        u: digikey('adafruit-industries-llc/4474/11587355'),
      },
      {
        s: 'Mouser',
        dist: 'mouser',
        us: 4.95,
        euNo: 'Mouser sells it only to business buyers in Europe',
        u: mouser('4474'),
      },
    ],
  },

  /* Fasteners: ettinger.de, net € per pack of 100. */
  {
    id: 'so20',
    pack: true,
    need: 4,
    name: 'Hex standoff M2.5 × 20, female/female',
    d: 'Brass, nickel-plated, SW 5 · 005.02.205',
    img: '/images/parts/s-so20.jpg',
    fit: true,
    shops: [
      {
        s: 'ettinger.de',
        eu: 19.6,
        u: ettinger(
          'abstandsbolzen-messing-vernickelt-i-i-sw5x20-gewinde-m2-5x6-m2-5x6',
          '005.02.205',
        ),
      },
    ],
  },
  {
    id: 'so11',
    pack: true,
    need: 4,
    name: 'Hex standoff M2.5 × 11, male/female',
    d: 'Stainless · 005.12.119',
    img: '/images/parts/s-so11.jpg',
    shops: [
      {
        s: 'ettinger.de',
        alt: 'Stainless, in stock',
        eu: 56.8,
        u: ettinger(
          'abstandsbolzen-stahl-rostfrei-blank-i-a-sw5x11-i-m2-5x7-a-m2-5x6-m.-freistich',
          '005.12.119',
        ),
      },
      {
        s: 'ettinger.de',
        alt: 'Brass, not in stock',
        d: 'Brass, about 100 days · 005.12.115',
        eu: 20.7,
        u: ettinger(
          'abstandsbolzen-messing-vernickelt-i-a-sw5x11-i-m2-5x6-a-m2-5x6-m.-freistich',
          '005.12.115',
        ),
      },
    ],
  },
  {
    id: 'washer25',
    pack: true,
    need: 4,
    needLaser: 12,
    name: 'Washer M2.5, nylon',
    d: 'One on each rear standoff, under the board · 003.03.026',
    img: '/images/parts/s-washer.jpg',
    shops: [
      {
        s: 'ettinger.de',
        eu: 4.2,
        u: ettinger('unterlegscheibe-iso7089-oe2-7-6-0-x-0-5-pa6.6-natur', '003.03.026'),
      },
    ],
  },
  {
    id: 'm2x18',
    pack: true,
    need: 4,
    name: 'Screw M2 × 18, cheese head, slotted',
    d: 'Holds the Lepton carrier · 001.14.177',
    img: '/images/parts/s-m2x18.jpg',
    shops: [
      {
        s: 'ettinger.de',
        eu: 5.15,
        u: ettinger('zylinderkopfschraube-iso1207-m2x18-stahl-4.8-verzinkt-schlitz', '001.14.177'),
      },
    ],
  },
  {
    id: 'm2washer',
    pack: true,
    need: 4,
    name: 'Washer M2, nylon',
    d: 'Black, under the M2 screw heads, off the board tracks · 003.03.010',
    img: '/images/parts/s-washer.jpg',
    shops: [
      {
        s: 'ettinger.de',
        eu: 8.4,
        u: ettinger('unterlegscheibe-oe2-2-5-0-x-0-3-pa6.0-schwarz', '003.03.010'),
      },
    ],
  },
  {
    id: 'sleeve',
    pack: true,
    need: 4,
    name: 'Spacer sleeve 2.2 × 4.0 × 12',
    d: 'Nylon, black · 005.81.120',
    img: '/images/parts/s-sleeve.jpg',
    shops: [
      {
        s: 'ettinger.de',
        eu: 4.55,
        u: ettinger('distanzhuelse-oe2-2-4-0-x-12-0-pa6.0-gv25-schwarz', '005.81.120'),
      },
    ],
  },
  {
    id: 'm2nut',
    pack: true,
    need: 4,
    name: 'Hex nut M2',
    d: 'Stainless A2 · 002.10.019',
    img: '/images/parts/s-m2nut.jpg',
    shops: [
      {
        s: 'ettinger.de',
        eu: 4.35,
        u: ettinger('sechskantmutter-iso4032-m2-stahl-rostfrei-a2-blank', '002.10.019'),
      },
    ],
  },
  {
    id: 'csk6',
    pack: true,
    need: 4,
    shell: ['print'],
    name: 'Countersunk screw M2.5 × 6',
    d: 'A2, Phillips PH1, back · 001.12.228',
    img: '/images/parts/s-csk.jpg',
    shops: [
      {
        s: 'ettinger.de',
        eu: 2.4,
        u: ettinger(
          'senkkopfschraube-iso7046-m2-5x6-stahl-rostfrei-a2-blank-phillips',
          '001.12.228',
        ),
      },
    ],
  },
  {
    id: 'csk14',
    pack: true,
    need: 4,
    shell: ['print'],
    name: 'Countersunk screw M2.5 × 14',
    d: 'A2, Phillips PH1, front · 082.22.212',
    img: '/images/parts/s-csk.jpg',
    shops: [
      {
        s: 'ettinger.de',
        eu: 3.15,
        u: ettinger(
          'senkkopfschraube-iso7046-m2-5x14-stahl-rostfrei-a2-blank-phillips',
          '082.22.212',
        ),
      },
    ],
  },
  {
    id: 'pan8',
    pack: true,
    need: 4,
    shell: ['laser'],
    name: 'Pan head screw M2.5 × 8',
    d: 'A2, Phillips PH1, back plate · 001.17.232',
    img: '/images/parts/s-pan.jpg',
    shops: [
      {
        s: 'ettinger.de',
        eu: 2.95,
        u: ettinger(
          'flachkopfschraube-iso7045-m2-5x8-stahl-rf-a2-blank-phillips-linsenkopf',
          '001.17.232',
        ),
      },
    ],
  },
  {
    id: 'pan14',
    pack: true,
    need: 4,
    shell: ['laser'],
    name: 'Pan head screw M2.5 × 14',
    d: 'A2, Phillips PH1, front plate · 001.17.298',
    img: '/images/parts/s-pan.jpg',
    shops: [
      {
        s: 'ettinger.de',
        eu: 7.2,
        u: ettinger(
          'flachkopfschraube-iso7045-m2-5x14-stahl-rf-a2-blank-phillips-linsenkopf',
          '001.17.298',
        ),
      },
    ],
  },
  {
    id: 'threadlocker',
    name: 'Threadlocker, medium strength',
    d: 'Removable, such as Loctite 243. A small drop on each Lepton screw tip.',
    icon: 'screw',
    anyIn: ['us', 'eu'],
    anyD: 'Medium-strength, removable, such as Loctite 243. A small drop on each Lepton screw tip.',
    shops: [],
  },
  {
    id: 'nut1420',
    name: 'Hex nut 1/4"-20 UNC',
    d: 'Zinc-plated steel, 7/16" across flats, 5.5 mm tall. Sold singly.',
    icon: 'nut',
    anyIn: ['us'],
    anyD: 'The standard tripod nut, 7/16" across flats. Every hardware store has it.',
    shops: [
      {
        s: 'zollshop.de',
        eu: 0.18,
        u: same(
          'https://www.zollshop.de/Zollmuttern-Scheiben-Federringe/Zollmuttern-UNC-UNF/Muttern-14-Zoll/14-Zoll-UNC/1061/1/4-Zoll-20-UNC-Sechskantmutter-Stahl-verzinkt/',
        ),
      },
    ],
  },
];

export interface Fab {
  name: string;
  url: string;
  dir: string;
  min: string;
  note: string;
  /** The Gerber zip's name after `diy-thermocam-v4.0-` (and `lepton-`); JLCPCB gets panels. */
  gerbers: string;
  /** Something to do before ordering the Lepton carrier from this fab. */
  carrierWarn?: string;
  drawings?: boolean;
  up: [string, string, string];
}

export const FAB: Record<'jlc' | 'seeed' | 'pcbway', Fab> = {
  jlc: {
    name: 'JLCPCB',
    url: 'https://jlcpcb.com/',
    dir: 'hardware/pcb/fab/jlcpcb/',
    gerbers: 'panel-gerbers.zip',
    min: 'Assembly starts at 2 boards.',
    note: 'Both boards use Standard (two-sided) assembly.',
    carrierWarn:
      "Every part comes from JLCPCB's own stock except the Lepton socket J102 (Molex 105028-1001, C585422). JLCPCB usually has none, so pre-order it in the parts manager first; the minimum is 9.",
    up: ['Gerber step', 'Assembly: BOM', 'Assembly: CPL'],
  },
  seeed: {
    name: 'Seeed Fusion',
    url: 'https://www.seeedstudio.com/fusion_pcb.html',
    dir: 'hardware/pcb/fab/seeed/',
    gerbers: 'gerbers.zip',
    min: 'Takes about 4 weeks.',
    note: 'Seeed buys the parts; the BOM links each one on LCSC. Also upload both assembly drawings.',
    drawings: true,
    up: ['PCB files', 'Assembly: BOM', 'Assembly: placement'],
  },
  pcbway: {
    name: 'PCBWay',
    url: 'https://www.pcbway.com/pcb-assembly.html',
    dir: 'hardware/pcb/fab/pcbway/',
    gerbers: 'gerbers.zip',
    min: '',
    note: 'PCBWay buys the parts; the notes column lists each LCSC number. Answer "no" to Chinese substitutes.',
    up: ['Gerber file', 'Assembly: BOM', 'Assembly: placement'],
  },
};

/* Who makes the shell. Researched 26 November 2026 on each service's own pages.
   No service publishes a price for an uploaded part except JLC3DP, which charges MJF PA12 by weight
   ($0.275 per gram, at least $1 per part). Its estimates below use the STL volumes in
   hardware/enclosure/exports at 1.01 g/cm³: the shell is 76.2 cm³ in 8 pieces, the acrylic case's
   printed parts 8.2 cm³ in 7 pieces, each under the $1 minimum. The others price the files on upload.
   Seeed's laser service is left out: its sheets stop at 20 × 20 cm and 2.55 mm acrylic, and the case
   sheet is 213.1 × 195.6 mm with slots for 2.8–3.2 mm. */
/* The printed parts, one file per part as the services want them (JLC3DP takes 10 files per upload,
   Craftcloud any number; both want one part per file). Folders are under hardware/enclosure/exports/. */
export interface PrintPart {
  file: string;
  qty: number;
  dir: '3d-printed' | 'laser-cut';
}
export const SHELL_PARTS: PrintPart[] = [
  { file: 'back_shell', qty: 1, dir: '3d-printed' },
  { file: 'front_shell', qty: 1, dir: '3d-printed' },
  { file: 'lens_cap', qty: 1, dir: '3d-printed' },
  { file: 'capture_button', qty: 1, dir: '3d-printed' },
  { file: 'slide_switch_cap', qty: 1, dir: '3d-printed' },
  { file: 'battery_cradle', qty: 1, dir: '3d-printed' },
  { file: 'battery_spacer', qty: 2, dir: '3d-printed' },
];
export const ACRYLIC_PRINT_PARTS: PrintPart[] = [
  { file: 'battery_cradle', qty: 1, dir: '3d-printed' },
  { file: 'battery_spacer', qty: 2, dir: '3d-printed' },
  { file: 'tripod_mount', qty: 1, dir: 'laser-cut' },
  { file: 'display_spacer', qty: 2, dir: 'laser-cut' },
  { file: 'sd_chute', qty: 1, dir: 'laser-cut' },
];

export interface Maker {
  id: string;
  name: string;
  kind: Shell;
  own?: boolean;
  url: string;
  short: string;
  how: string;
  where: string;
  /** What to pick on the service's order form, in its own words. */
  order?: string[];
  /** Laser services: which sheet file to upload. */
  file?: 'svg' | 'dxf';
  /** Estimated price in $ for the full shell, and for the acrylic case's small printed parts. */
  est?: { shell: number; small: number; basis: string };
}

export const MAKERS: Maker[] = [
  {
    id: 'self',
    kind: 'print',
    own: true,
    name: 'My 3D printer',
    url: '',
    short: 'PETG, under 100 g of filament',
    how: 'PETG, 0.4 mm nozzle, 0.2 mm layers, 3–4 walls. The back shell needs supports from the plate.',
    where: '',
  },
  {
    id: 'jlc3dp',
    kind: 'print',
    name: 'JLC3DP',
    url: 'https://jlc3dp.com/',
    short: 'MJF nylon, from China, price by weight',
    how: 'MJF PA12 nylon, dyed black.',
    where: 'Ships worldwide from China, in the same cart as JLCPCB.',
    order: [
      'Technology: MJF (Nylon). Material: PA12, or PA12S if it is offered (it takes the dye more evenly).',
      'Color: Black, dyed. That is the matte black in the pictures.',
      'Surface finish or sanding: No. Thread: No: every screw goes into a metal standoff or nut.',
      'Product description: the plastic enclosure entry; it is only for customs.',
      "Not SLA resin, even on discount: JLC3DP's Imagine Black is made for miniatures, and its Black Resin softens at 58–65 °C, which a camera in a hot car can reach.",
    ],
    est: {
      shell: 25.43,
      small: 7,
      basis: '$0.275 per gram, at least $1 per part, before dyeing and shipping',
    },
  },
  {
    id: 'pcbway3d',
    kind: 'print',
    name: 'PCBWay',
    url: 'https://www.pcbway.com/rapid-prototyping/3d-printing/',
    short: 'MJF or SLS nylon, from China',
    how: 'MJF or SLS PA12 nylon, dyed black.',
    where: 'Ships worldwide from China, in the same cart as a PCBWay board.',
  },
  {
    id: 'seeed3d',
    kind: 'print',
    name: 'Seeed Fusion',
    url: 'https://www.seeedstudio.com/fusion-3d-printing.html',
    short: 'Nylon or resin, from China',
    how: 'PA12 nylon, or SLA resin for a smoother face.',
    where: 'Ships worldwide from China.',
  },
  {
    id: 'craftcloud',
    kind: 'print',
    name: 'Craftcloud',
    url: 'https://craftcloud3d.com/',
    short: 'Compares print shops in the US and Europe',
    how: 'Pick PA12 nylon or PETG; it lists offers from many print shops.',
    where: 'Offers from shops near you in the US and Europe.',
  },
  {
    id: 'selflaser',
    kind: 'laser',
    own: true,
    name: 'My laser cutter',
    url: '',
    short: '3 mm black cast acrylic, 220 × 200 mm',
    how: 'One 220 × 200 mm blank of 3 mm black cast acrylic. Slots fit 2.8–3.2 mm.',
    where: '',
    file: 'svg',
  },
  {
    id: 'formulor',
    kind: 'laser',
    name: 'Formulor',
    url: 'https://www.formulor.de/en/',
    short: 'Berlin, ships in Europe',
    how: 'Acrylic GS black, opaque, 3 mm, sheet format P2 (384 × 384 mm). No minimum order.',
    where: 'Ships from Germany: €5.90 within Germany, free from €50.',
    file: 'svg',
  },
  {
    id: 'snijlab',
    kind: 'laser',
    name: 'Snijlab',
    url: 'https://snijlab.nl/en',
    short: 'Rotterdam, ships in Europe',
    how: 'Black cast acrylic, 3 mm.',
    where: 'Ships from the Netherlands.',
    file: 'svg',
  },
  {
    id: 'ponoko',
    kind: 'laser',
    name: 'Ponoko',
    url: 'https://www.ponoko.com/laser-cutting/acrylic',
    short: 'US, ships worldwide',
    how: 'Black acrylic, 3 mm. No minimum order.',
    where: 'Ships from the US, worldwide.',
    file: 'svg',
  },
  {
    id: 'sendcutsend',
    kind: 'laser',
    name: 'SendCutSend',
    url: 'https://sendcutsend.com/materials/acrylic/',
    short: 'US, takes the DXF',
    how: 'Black acrylic, 0.118" (3 mm). Made in 2–4 days.',
    where: 'Ships within the US.',
    file: 'dxf',
  },
  {
    id: 'elecrow',
    kind: 'laser',
    name: 'Elecrow',
    url: 'https://www.elecrow.com/acrylic-cutting.html',
    short: 'China, ships worldwide',
    how: 'Black acrylic, 3 mm. Made in 2–4 working days.',
    where: 'Ships worldwide from China.',
    file: 'dxf',
  },
];
