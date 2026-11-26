import {
  ACRYLIC_PRINT_PARTS,
  FAB,
  MAKERS,
  type Maker,
  type Offer,
  PARTS,
  type Part,
  type PrintPart,
  type Region,
  SHELL_PARTS,
  type Shell,
  USD_PER_EUR,
} from '../data/parts';

const GH = 'https://github.com/maxritter/diy-thermocam/raw/master/';
const TREE = 'https://github.com/maxritter/diy-thermocam/tree/master/';
const F = 'diy-thermocam-v4.0-';

const ICON: Record<string, string> = {
  header:
    '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" aria-hidden="true"><rect x="2" y="13" width="20" height="5" rx="1"/><path d="M4.5 13V5M8.5 13V5M12 13V5M15.5 13V5M19.5 13V5M4.5 18v3M8.5 18v3M12 18v3M15.5 18v3M19.5 18v3"/></svg>',
  screw:
    '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" aria-hidden="true"><path d="M6 4h12l-6 4z"/><path d="M12 8v13M9 11l6-1M9 14l6-1M9 17l6-1"/></svg>',
  nut: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linejoin="round" aria-hidden="true"><path d="M7 3.5h10l5 8.5-5 8.5H7L2 12z"/><circle cx="12" cy="12" r="3.5"/></svg>',
};

const form = document.getElementById('cfg') as HTMLFormElement | null;
const orders = document.getElementById('orders');
const totalEl = document.getElementById('total');
const plusEl = document.getElementById('plus');
const statusEl = document.getElementById('copy-status');

if (form && orders && totalEl && plusEl && statusEl) {
  let chosenShop: Record<string, number> = {};
  let region: Region = 'us';
  try {
    if (
      /^(de|fr|es|it|nl|pt|fi|el|sk|sl|et|lv|lt|ga|mt|pl|cs|da|sv|hu|ro|bg|hr)\b/i.test(
        navigator.language,
      )
    )
      region = 'eu';
  } catch {
    /* keep US */
  }
  const regionInput = document.querySelector<HTMLInputElement>(
    `input[name="cur"][value="${region}"]`,
  );
  if (regionInput) regionInput.checked = true;

  const esc = (s: string) =>
    s.replace(
      /[&<>"]/g,
      (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c] as string,
    );
  const money = (n: number) => (region === 'eu' ? '€' : '$') + n.toFixed(2);
  /* The offer's price in the chosen region: the store's own price, else the other region's converted. */
  function priceOf(o: Offer): { v: number; conv?: boolean } | null {
    if (region === 'eu')
      return o.eu != null
        ? { v: o.eu }
        : o.us != null
          ? { v: o.us / USD_PER_EUR, conv: true }
          : null;
    return o.us != null ? { v: o.us } : o.eu != null ? { v: o.eu * USD_PER_EUR, conv: true } : null;
  }
  const usable = (o: Offer) => !(region === 'eu' && o.euNo);
  function state() {
    const fd = new FormData(form as HTMLFormElement);
    return {
      own: fd.getAll('own') as string[],
      sensor: String(fd.get('sensor')),
      fab: String(fd.get('fab')) as keyof typeof FAB,
      shell: String(fd.get('shell')) as Shell,
      pmaker: String(fd.get('pmaker')),
      lmaker: String(fd.get('lmaker')),
      smaker: String(fd.get('smaker')),
      dist: String(fd.get('dist')),
    };
  }
  function thumb(p: { img?: string; fit?: boolean; icon?: string }) {
    if (p.img)
      return `<span class="thumb${p.fit ? ' fit' : ''}"><img src="${p.img}" alt="" loading="lazy"></span>`;
    return `<span class="thumb">${ICON[p.icon ?? 'screw']}</span>`;
  }
  function fileRow(path: string, where: string, label = 'Upload at') {
    const folder = path.endsWith('/');
    const name = folder
      ? `${path.replace(/\/$/, '').split('/').pop()}/`
      : (path.split('/').pop() ?? path);
    const href = folder ? TREE + path : GH + path;
    return `<div class="file"><span><code>${esc(name)}</code><small>${esc(label)}: ${esc(where)}</small></span><a href="${href}">${folder ? 'Open folder' : 'Download'}</a></div>`;
  }

  interface Row {
    html: string;
    price?: number | null;
    own?: boolean;
    pending?: boolean;
  }
  function render() {
    const st = state();
    for (const fs of (form as HTMLFormElement).querySelectorAll<HTMLFieldSetElement>(
      'fieldset[data-kind]',
    ))
      fs.hidden = fs.dataset.kind !== st.shell;
    const fab = FAB[st.fab];
    const groups: Record<string, { rows: Row[]; extra: string }> = {};
    const order: string[] = [];
    let total = 0;
    let converted = false;
    const add = (shop: string, entry: Row) => {
      if (!groups[shop]) {
        groups[shop] = { rows: [], extra: '' };
        order.push(shop);
      }
      groups[shop].rows.push(entry);
    };

    /* Board fab: the board and the files with their upload step. */
    add(fab.name, {
      html: `<div class="prow">${thumb({ img: '/images/v4/pcb-modules.png', fit: true })}<span><b>V4 board, assembled</b><span class="d">${esc(fab.note)} ${esc(fab.min)}</span><span class="links"><a href="${fab.url}">${esc(fab.name)}</a></span></span><span></span><span class="p">Quote<small>at checkout</small></span></div>`,
    });
    let files =
      fileRow(`${fab.dir}${F}gerbers.zip`, fab.up[0]) +
      fileRow(`${fab.dir}${F}bom.csv`, fab.up[1]) +
      fileRow(`${fab.dir}${F}cpl.csv`, fab.up[2]);
    if (fab.drawings) {
      files +=
        fileRow(`hardware/pcb/fab/${F}assembly-bottom.pdf`, 'Assembly drawings') +
        fileRow(`hardware/pcb/fab/${F}assembly-top.pdf`, 'Assembly drawings');
    }
    const settings = [
      '2 layers, 89.4 × 68.4 mm, 1.6 mm FR-4, 1 oz copper, lead-free HASL or ENIG, any colour',
      'Assembly on both sides: everything on the bottom, the display socket J1 on top',
      `Before you pay, compare the placement preview with <a href="${GH}hardware/pcb/fab/${F}assembly-bottom.pdf">the bottom assembly drawing</a>: pin 1 of U2, U3, U4, D1, D2, Q1 and Q2 must line up.`,
    ];
    groups[fab.name].extra =
      `<div class="files"><b class="h">Files to upload</b>${files}<ul class="settings">${settings.map((s) => `<li>${s}</li>`).join('')}</ul></div>`;

    /* The shell: who makes it, what to order there, the price and the files. */
    const PRINTED = 'hardware/enclosure/exports/3d-printed/';
    const LASER = 'hardware/enclosure/exports/laser-cut/';
    const maker = (id: string, kind: Shell) =>
      MAKERS.find((m) => m.id === id && m.kind === kind) ??
      (MAKERS.find((m) => m.kind === kind) as Maker);
    let quoted = false;
    const shellRow = (m: Maker, what: string, img: string, est: number | undefined, f: string) => {
      let price: string;
      if (m.own) price = m.kind === 'print' ? 'Your filament' : 'Your acrylic';
      else if (est != null) {
        price = `${money(region === 'eu' ? est / USD_PER_EUR : est)}<small>estimate</small>`;
        quoted = true;
      } else {
        price = 'Quote<small>on upload</small>';
        quoted = true;
      }
      const basis = !m.own && est != null && m.est ? ` Estimate: ${m.est.basis}.` : '';
      add(m.name, {
        own: m.own,
        html: `<div class="prow">${thumb({ img })}<span><b>${esc(what)}</b><span class="d">${esc(m.how)}${m.where ? ` ${esc(m.where)}` : ''}${esc(basis)}</span>${m.url ? `<span class="links"><a href="${esc(m.url)}">Open ${esc(m.name)}</a></span>` : ''}</span><span></span><span class="p">${price}</span></div>`,
      });
      groups[m.name].extra += f;
    };
    const filesBox = (title: string, rows: string) =>
      `<div class="files"><b class="h">${title}</b>${rows}</div>`;
    /* One line per printed part: its quantity and the file in each format. */
    const partFiles = (parts: PrintPart[], note: string) => {
      const rows = parts
        .map((pp) => {
          const base = `hardware/enclosure/exports/${pp.dir}/${F}${pp.file}`;
          const links = ['step', 'stl', '3mf']
            .map((x) => `<a href="${GH}${base}.${x}">${x.toUpperCase()}</a>`)
            .join('');
          return `<div class="pf"><code>${esc(pp.file)}</code><span class="q">× ${pp.qty}</span><span class="fl">${links}</span></div>`;
        })
        .join('');
      return `<p class="pf-note">${esc(note)}</p><div class="pfs">${rows}</div>`;
    };
    /* Services get the four C-spacers as one part on break-off tabs, so every quantity stays 1. */
    const forService = (parts: PrintPart[]) =>
      parts.map((pp) =>
        pp.file === 'cradle_spacer' ? { ...pp, file: 'cradle_spacer_set', qty: 1 } : pp,
      );
    const TABS =
      'The four C-spacers come joined on break-off tabs: snip them off and file the nubs flush.';
    /* JLC3DP splits one combined file into its parts; other services want one file per part. */
    const serviceFiles = (
      m: Maker,
      plate: string,
      zip: string,
      parts: PrintPart[],
      count: number,
    ) =>
      filesBox(
        'Files to upload',
        (m.id === 'jlc3dp'
          ? fileRow(
              plate,
              `JLC3DP. One file: it splits into ${count} named parts, each quantity 1.`,
            )
          : '') +
          fileRow(
            zip,
            `${m.name}${m.id === 'jlc3dp' ? ', or any other service' : ''}. Unzip, then upload the step folder (or stl, not both).`,
            m.id === 'jlc3dp' ? 'Or upload' : 'Upload at',
          ) +
          partFiles(forService(parts), `Or single files. ${TABS}`) +
          `<b class="h sub">Choose on the order form</b><ul class="settings">${(
            m.order ?? [
              'MJF or SLS PA12 nylon, black (dyed). That is the matte black in the pictures.',
              'No sanding and no threads: every screw goes into a metal standoff or nut.',
              'Avoid standard resins: they get brittle and soften in a hot car.',
            ]
          )
            .map((s) => `<li>${esc(s)}</li>`)
            .join('')}</ul>`,
      );

    if (st.shell === 'print') {
      const m = maker(st.pmaker, 'print');
      shellRow(
        m,
        m.own ? 'Shell, every part on one plate' : 'Shell, 8 parts',
        '/images/v4/view-kit-thumb.jpg',
        m.est?.shell,
        m.own
          ? filesBox(
              'Files to print',
              fileRow(
                `${PRINTED}${F}print-plate.3mf`,
                'your slicer, every part at its quantity',
                'Open in',
              ) + partFiles(SHELL_PARTS, 'Or print single parts:'),
            )
          : serviceFiles(
              m,
              `${PRINTED}${F}service-plate.3mf`,
              `${PRINTED}${F}shell-print-files.zip`,
              SHELL_PARTS,
              8,
            ),
      );
    } else {
      const m = maker(st.lmaker, 'laser');
      const sheet = `${LASER}${F}laser-sheet.${m.file ?? 'svg'}`;
      shellRow(
        m,
        'Acrylic case, one 3 mm sheet',
        '/images/v4/laser-sheet.jpg',
        undefined,
        filesBox(
          m.own ? 'File to cut' : 'File to upload',
          fileRow(
            sheet,
            `${m.own ? 'your laser' : m.name}. Red lines cut, blue lines engrave the logo (optional). The sheet is 213 × 195 mm.`,
            m.own ? 'Open in' : 'Upload at',
          ),
        ),
      );
      const s = maker(st.smaker, 'print');
      shellRow(
        s,
        '10 small printed parts',
        '/images/v4/laser-kit.jpg',
        s.est?.small,
        s.own
          ? filesBox(
              'Files to print',
              fileRow(
                `${LASER}${F}laser-print-plate.3mf`,
                'your slicer, every part at its quantity',
                'Open in',
              ) + partFiles(ACRYLIC_PRINT_PARTS, 'Or print single parts:'),
            )
          : serviceFiles(
              s,
              `${LASER}${F}laser-service-plate.3mf`,
              `${LASER}${F}acrylic-files.zip`,
              ACRYLIC_PRINT_PARTS,
              7,
            ),
      );
    }

    for (const p of PARTS as Part[]) {
      if (p.own && st.own.includes(p.own)) continue;
      if (p.sensor && p.sensor !== st.sensor) continue;
      if (p.shell && !p.shell.includes(st.shell as never)) continue;
      if (p.anyIn?.includes(region)) {
        const ex = p.shops.find((s) => s[region] != null);
        add('Any shop', {
          own: true,
          html: `<div class="prow">${thumb(p)}<span><b>${esc(p.name)}</b><span class="d">${esc(p.anyD || p.d)}</span>${ex ? `<span class="links"><a href="${esc(ex.u[region])}">For example at ${esc(ex.s)}</a></span>` : ''}</span><span></span><span class="p">Any shop</span></div>`,
        });
        continue;
      }
      /* Default: the chosen distributor if it sells this part here, else the first shop that does. */
      let def = p.shops.findIndex((o) => o.dist === st.dist && usable(o));
      if (def < 0) def = p.shops.findIndex(usable);
      if (def < 0) def = 0;
      const need = st.shell === 'laser' && p.needLaser ? p.needLaser : p.need;
      const i = chosenShop[p.id] ?? def;
      const o = p.shops[i];
      const pr = o.pending ? null : priceOf(o);
      const line = pr ? pr.v : null;
      const sel =
        p.shops.length > 1
          ? `<select aria-label="Shop for ${esc(p.name)}" data-part="${p.id}">${p.shops.map((s, k) => `<option value="${k}"${k === i ? ' selected' : ''}${usable(s) ? '' : ' disabled'}>${esc(s.alt || s.s)}${usable(s) ? '' : ' (not in Europe)'}</option>`).join('')}</select>`
          : '<span></span>';
      const link = `<a href="${esc(o.u[region])}">Open at ${esc(o.s)}</a>`;
      const alt = p.shops
        .filter((s, k) => k !== i && s.s !== o.s && usable(s))
        .map((s) => `<a href="${esc(s.u[region])}">or ${esc(s.s)}</a>`)
        .join('');
      let notes = '';
      if (region === 'eu' && p.euNote) notes += `<span class="warn">${esc(p.euNote)}</span>`;
      if (region === 'eu' && i !== def && o.euNo)
        notes += `<span class="warn">${esc(o.euNo)}</span>`;
      const warn = o.warn ?? p.warn;
      if (warn) notes += `<span class="warn">${esc(warn)}</span>`;
      if (region === 'us' && p.usNote) notes += `<span class="d">${esc(p.usNote)}</span>`;
      if (region === 'eu' && st.dist === 'mouser' && o.dist !== 'mouser') {
        const m = p.shops.find((s) => s.dist === 'mouser');
        if (m?.euNo)
          notes += `<span class="d">${esc(m.euNo)}, so it comes from ${esc(o.s)}.</span>`;
      }
      let priceHtml: string;
      if (line == null) priceHtml = '<span class="pending">Price to come</span>';
      else {
        priceHtml =
          money(line) +
          (p.pack ? '<small>pack of 100</small>' : pr?.conv ? '<small>converted</small>' : '');
        total += line;
        if (pr?.conv) converted = true;
      }
      add(o.s, {
        price: line,
        pending: !!o.pending,
        html: `<div class="prow">${thumb(p)}<span><b>${need ? `${need}× ` : ''}${esc(o.n || p.name)}</b><span class="d">${esc(o.d || p.d)}</span>${notes}<span class="links">${link}${alt}</span></span>${sel}<span class="p">${priceHtml}</span></div>`,
      });
    }

    let html = '';
    let shops = 0;
    order.sort((a, b) => Number(a === 'Any shop') - Number(b === 'Any shop'));
    for (const name of order) {
      const g = groups[name];
      if (!g.rows.every((r) => r.own || r.pending)) shops++;
      const sub = g.rows.reduce((a, r) => a + (r.price || 0), 0);
      const note =
        name === 'ettinger.de'
          ? `<span class="n">Packs of 100, net prices${region === 'us' ? ', ships from Germany' : ''}. Sign in to order.</span>`
          : name === 'Any shop'
            ? '<span class="n">Not in the total</span>'
            : '';
      html += `<section class="shop" aria-label="${esc(name)}"><div class="shop-h"><h3>${esc(name)}</h3><span class="n">${g.rows.length}${g.rows.length === 1 ? ' item' : ' items'}</span>${note}<span class="st">${sub ? money(sub) : ''}</span></div>${g.rows.map((r) => r.html).join('')}${g.extra || ''}</section>`;
    }
    (orders as HTMLElement).innerHTML =
      html || '<div class="empty">Nothing to buy. You have every part.</div>';
    (totalEl as HTMLElement).textContent = money(total);
    (plusEl as HTMLElement).textContent =
      `${shops} orders · plus the board${quoted ? ' and the shell' : ''} at checkout${converted ? ` · converted at €1 = $${USD_PER_EUR.toFixed(2)}` : ''}`;
    for (const s of (orders as HTMLElement).querySelectorAll<HTMLSelectElement>(
      'select[data-part]',
    )) {
      s.addEventListener('change', () => {
        const id = s.getAttribute('data-part') as string;
        chosenShop[id] = Number(s.value);
        render();
        (orders as HTMLElement)
          .querySelector<HTMLSelectElement>(`select[data-part="${id}"]`)
          ?.focus();
      });
    }
  }

  form.addEventListener('change', (e) => {
    statusEl.textContent = '';
    if ((e.target as HTMLInputElement).name === 'dist') chosenShop = {};
    render();
  });
  for (const r of document.querySelectorAll<HTMLInputElement>('input[name="cur"]')) {
    r.addEventListener('change', () => {
      if (!r.checked) return;
      region = r.value as Region;
      chosenShop = {};
      statusEl.textContent = '';
      render();
    });
  }
  document.getElementById('reset')?.addEventListener('click', () => {
    form.reset();
    chosenShop = {};
    statusEl.textContent = '';
    render();
  });
  document.getElementById('copy')?.addEventListener('click', () => {
    const text = [...orders.querySelectorAll('.shop')]
      .map((s) => {
        const head = s.querySelector('h3')?.textContent ?? '';
        const rows = [...s.querySelectorAll('.prow')].map((r) => {
          const b = r.querySelector('b');
          const a = r.querySelector<HTMLAnchorElement>('.links a');
          const p = r.querySelector('.p');
          const price = p?.firstChild?.textContent ?? '';
          return `- ${b?.textContent ?? ''}${price ? `, ${price}` : ''}${a ? `\n  ${a.href}` : ''}`;
        });
        const fl = [
          ...[...s.querySelectorAll('.file')].map(
            (x) =>
              `  file: ${x.querySelector('code')?.textContent ?? ''} ${x.querySelector<HTMLAnchorElement>('a')?.href ?? ''}`,
          ),
          ...[...s.querySelectorAll('.pf')].map(
            (x) =>
              `  part: ${x.querySelector('code')?.textContent ?? ''} ${x.querySelector('.q')?.textContent ?? ''} ${x.querySelector<HTMLAnchorElement>('a')?.href ?? ''}`,
          ),
        ];
        return `${head}\n${rows.join('\n')}${fl.length ? `\n${fl.join('\n')}` : ''}`;
      })
      .join('\n\n');
    const done = (ok: boolean) => {
      statusEl.textContent = ok
        ? 'Copied the list.'
        : "Couldn't copy here. Select the list and copy it by hand.";
    };
    const legacy = () => {
      const ta = document.createElement('textarea');
      ta.value = text;
      ta.setAttribute('readonly', '');
      ta.style.position = 'fixed';
      ta.style.opacity = '0';
      document.body.appendChild(ta);
      ta.select();
      let ok = false;
      try {
        ok = document.execCommand('copy');
      } catch {
        ok = false;
      }
      document.body.removeChild(ta);
      done(ok);
    };
    if (navigator.clipboard?.writeText)
      navigator.clipboard.writeText(text).then(() => done(true), legacy);
    else legacy();
  });
  render();
}
