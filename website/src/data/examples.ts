/* Example scenes: generated 160 × 120 radiometric frames, rendered the way Studio exports them.
   Each has a matching .DAT in /examples/ that Studio's editor opens. V4 has no real captures yet. */
export interface Example {
  id: string;
  title: string;
  dat: string;
  palette: string;
  range: string;
  note: string;
}

export const EXAMPLES: Example[] = [
  {
    id: 'house',
    title: 'House front in winter',
    dat: 'HOUSE.DAT',
    palette: 'Ironblack',
    range: '−3 to 11 °C',
    note: 'Heat escapes around every window frame and under the eaves.',
  },
  {
    id: 'pcb',
    title: 'Board under load',
    dat: 'PCB.DAT',
    palette: 'Inferno',
    range: '24 to 86 °C',
    note: 'The processor runs at 86 °C, the regulator next to it is the second hot spot.',
  },
  {
    id: 'person',
    title: 'Person',
    dat: 'PERSON.DAT',
    palette: 'Ironblack',
    range: '21 to 36 °C',
    note: 'Glasses block long-wave infrared, so they show up cold.',
  },
  {
    id: 'radiator',
    title: 'Radiator',
    dat: 'RADIATOR.DAT',
    palette: 'Rainbow',
    range: '17 to 56 °C',
    note: 'Warm channels from the supply pipe, cold window glass above.',
  },
  {
    id: 'breaker',
    title: 'Breaker panel',
    dat: 'BREAKER.DAT',
    palette: 'Ironblack',
    range: '22 to 74 °C',
    note: 'One overloaded breaker and its cable stand out from the rest.',
  },
  {
    id: 'coffee',
    title: 'Coffee',
    dat: 'COFFEE.DAT',
    palette: 'Ironblack',
    range: '19 to 68 °C',
    note: 'The surface of the coffee is the hottest point; the handle stays cool.',
  },
  {
    id: 'dog',
    title: 'Dog',
    dat: 'DOG.DAT',
    palette: 'Lava',
    range: '17 to 38 °C',
    note: 'Eyes, ears and nose are the warmest parts of a short-haired dog.',
  },
  {
    id: 'floor',
    title: 'Underfloor heating',
    dat: 'FLOOR.DAT',
    palette: 'Plasma',
    range: '20 to 34 °C',
    note: 'The pipe loops show through the tiles, warmest near the manifold.',
  },
];

export const EXAMPLE_W = 1470;
export const EXAMPLE_H = 1024;
