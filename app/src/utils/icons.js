const X = 'xmlns="http://www.w3.org/2000/svg"'
const R = 'stroke-linecap="round" stroke-linejoin="round"'

function sk(d) {
  return (c = '#e0e0e0') =>
    `<svg ${X} viewBox="0 0 24 24" fill="none" stroke="${c}" stroke-width="2" ${R}>${d}</svg>`
}
function fl(d) {
  return (c = '#e0e0e0') =>
    `<svg ${X} viewBox="0 0 24 24">${d.replace(/_/g, c)}</svg>`
}

const I = {
  mosquito: fl(`<g fill="_" stroke="_" stroke-width=".5"><circle cx="12" cy="5" r="1.8"/><ellipse cx="12" cy="11" rx="2.5" ry="4.5"/><line x1="12" y1="15.5" x2="12" y2="22" stroke-width="1.5"/><path d="M9.5 8Q6 5 4.5 7" fill="none" stroke-width="1.2"/><path d="M14.5 8Q18 5 19.5 7" fill="none" stroke-width="1.2"/><path d="M10 10L6.5 14M14 10L17.5 14" fill="none" stroke-width=".8"/><path d="M10.5 12.5L7 17M13.5 12.5L17 17" fill="none" stroke-width=".8"/></g>`),

  'fruit-fly': fl(`<g fill="_" stroke="_" stroke-width=".5"><circle cx="12" cy="14" r="3.2"/><circle cx="12" cy="9.5" r="2"/><ellipse cx="7" cy="10.5" rx="3" ry="1.8" transform="rotate(-20 7 10.5)" fill="none" stroke-width="1.2"/><ellipse cx="17" cy="10.5" rx="3" ry="1.8" transform="rotate(20 17 10.5)" fill="none" stroke-width="1.2"/><path d="M10.5 8L9 4.5M13.5 8L15 4.5" fill="none" stroke-width="1"/></g>`),

  fly: fl(`<g fill="_" stroke="_" stroke-width=".5"><ellipse cx="12" cy="14.5" rx="3.5" ry="4"/><circle cx="12" cy="9.5" r="2.5"/><ellipse cx="6.5" cy="11" rx="3.5" ry="2" transform="rotate(-15 6.5 11)" fill="none" stroke-width="1.2"/><ellipse cx="17.5" cy="11" rx="3.5" ry="2" transform="rotate(15 17.5 11)" fill="none" stroke-width="1.2"/><path d="M9.5 16L6 20M14.5 16L18 20M10 17.5L7 22M14 17.5L17 22" fill="none" stroke-width=".8"/></g>`),

  moth: fl(`<g fill="_" stroke="_" stroke-width=".5"><ellipse cx="12" cy="14" rx="1.5" ry="4"/><circle cx="12" cy="9" r="1.5"/><path d="M10.5 11C7 8 3.5 7 3 11s2 7 7 4" stroke-width="1"/><path d="M13.5 11C17 8 20.5 7 21 11s-2 7-7 4" stroke-width="1"/><path d="M11 8Q8.5 3.5 7.5 2" fill="none" stroke-width="1.2"/><path d="M13 8Q15.5 3.5 16.5 2" fill="none" stroke-width="1.2"/><circle cx="7.5" cy="2" r=".8"/><circle cx="16.5" cy="2" r=".8"/></g>`),

  fire: sk('<path d="M12 2c-2 4-5.5 6.5-5.5 11.5a5.5 5.5 0 0011 0C17.5 8.5 14 6 12 2z"/><path d="M12 22a2.5 2.5 0 002.5-2.5c0-1.5-2.5-3.5-2.5-3.5s-2.5 2-2.5 3.5A2.5 2.5 0 0012 22z"/>'),

  lightning: sk('<polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"/>'),

  thermometer: sk('<path d="M14 14.76V3.5a2.5 2.5 0 00-5 0v11.26a4.5 4.5 0 105 0z"/>'),

  droplet: sk('<path d="M12 2.69l5.66 5.66a8 8 0 11-11.31 0z"/>'),

  sun: sk('<circle cx="12" cy="12" r="5"/><line x1="12" y1="1" x2="12" y2="3"/><line x1="12" y1="21" x2="12" y2="23"/><line x1="4.22" y1="4.22" x2="5.64" y2="5.64"/><line x1="18.36" y1="18.36" x2="19.78" y2="19.78"/><line x1="1" y1="12" x2="3" y2="12"/><line x1="21" y1="12" x2="23" y2="12"/><line x1="4.22" y1="19.78" x2="5.64" y2="18.36"/><line x1="18.36" y1="5.64" x2="19.78" y2="4.22"/>'),

  moon: sk('<path d="M21 12.79A9 9 0 1111.21 3 7 7 0 0021 12.79z"/>'),

  skull: sk('<circle cx="12" cy="10" r="7"/><circle cx="9.5" cy="10" r="1.5"/><circle cx="14.5" cy="10" r="1.5"/><path d="M9 17v3M15 17v3M9 17h6"/>'),

  trophy: sk('<path d="M6 9H3V5h3M18 9h3V5h-3"/><path d="M6 5h12v6a6 6 0 01-12 0V5z"/><line x1="12" y1="17" x2="12" y2="20"/><line x1="8" y1="22" x2="16" y2="22"/>'),

  gear: sk('<circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 00.33 1.82l.06.06a2 2 0 010 2.83 2 2 0 01-2.83 0l-.06-.06a1.65 1.65 0 00-1.82-.33 1.65 1.65 0 00-1 1.51V21a2 2 0 01-4 0v-.09A1.65 1.65 0 009 19.4a1.65 1.65 0 00-1.82.33l-.06.06a2 2 0 01-2.83-2.83l.06-.06A1.65 1.65 0 004.68 15a1.65 1.65 0 00-1.51-1H3a2 2 0 010-4h.09A1.65 1.65 0 004.6 9a1.65 1.65 0 00-.33-1.82l-.06-.06a2 2 0 012.83-2.83l.06.06A1.65 1.65 0 009 4.68a1.65 1.65 0 001-1.51V3a2 2 0 014 0v.09a1.65 1.65 0 001 1.51 1.65 1.65 0 001.82-.33l.06-.06a2 2 0 012.83 2.83l-.06.06A1.65 1.65 0 0019.4 9a1.65 1.65 0 001.51 1H21a2 2 0 010 4h-.09a1.65 1.65 0 00-1.51 1z"/>'),

  clock: sk('<circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/>'),

  plug: sk('<path d="M12 22v-4"/><path d="M7 12V6"/><path d="M17 12V6"/><rect x="5" y="12" width="14" height="4" rx="2"/>'),

  speaker: sk('<polygon points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"/><path d="M15.54 8.46a5 5 0 010 7.07"/><path d="M19.07 4.93a10 10 0 010 14.14"/>'),

  bulb: sk('<path d="M9 18h6"/><path d="M10 22h4"/><path d="M15.09 14c.18-.98.65-1.74 1.41-2.5A4.65 4.65 0 0018 8 6 6 0 006 8c0 1 .23 2.23 1.5 3.5A4.61 4.61 0 018.91 14"/>'),

  package: sk('<path d="M16.5 9.4l-9-5.19M21 16V8a2 2 0 00-1-1.73l-7-4a2 2 0 00-2 0l-7 4A2 2 0 003 8v8a2 2 0 001 1.73l7 4a2 2 0 002 0l7-4A2 2 0 0021 16z"/><polyline points="3.27 6.96 12 12.01 20.73 6.96"/><line x1="12" y1="22.08" x2="12" y2="12"/>'),

  save: sk('<path d="M19 21H5a2 2 0 01-2-2V5a2 2 0 012-2h11l5 5v11a2 2 0 01-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/><polyline points="7 3 7 8 15 8"/>'),

  check: sk('<polyline points="20 6 9 17 4 12"/>'),

  'check-circle': sk('<path d="M22 11.08V12a10 10 0 11-5.93-9.14"/><polyline points="22 4 12 14.01 9 11.01"/>'),

  cross: sk('<line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/>'),

  'x-circle': sk('<circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/>'),

  warning: sk('<path d="M10.29 3.86L1.82 18a2 2 0 001.71 3h16.94a2 2 0 001.71-3L13.71 3.86a2 2 0 00-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/>'),

  refresh: sk('<polyline points="23 4 23 10 17 10"/><polyline points="1 20 1 14 7 14"/><path d="M3.51 9a9 9 0 0114.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0020.49 15"/>'),

  back: sk('<line x1="19" y1="12" x2="5" y2="12"/><polyline points="12 19 5 12 12 5"/>'),

  radar: sk('<circle cx="12" cy="12" r="2"/><path d="M12 2a10 10 0 0110 10"/><path d="M12 2a10 10 0 00-10 10"/><path d="M12 6a6 6 0 016 6"/><path d="M12 6a6 6 0 00-6 6"/>'),

  bluetooth: sk('<polyline points="6.5 6.5 17.5 17.5 12 23 12 1 17.5 6.5 6.5 17.5"/>'),

  chart: sk('<line x1="18" y1="20" x2="18" y2="10"/><line x1="12" y1="20" x2="12" y2="4"/><line x1="6" y1="20" x2="6" y2="14"/>'),

  sparkle: sk('<path d="M12 2l2.4 7.2L22 12l-7.6 2.8L12 22l-2.4-7.2L2 12l7.6-2.8z"/>'),

  hourglass: sk('<path d="M5 3h14M5 21h14"/><path d="M7 3v3a5 5 0 005 5 5 5 0 005-5V3"/><path d="M7 21v-3a5 5 0 015-5 5 5 0 015 5v3"/>'),

  home: sk('<path d="M3 9l9-7 9 7v11a2 2 0 01-2 2H5a2 2 0 01-2-2z"/><polyline points="9 22 9 12 15 12 15 22"/>'),

  'bar-chart': sk('<line x1="12" y1="20" x2="12" y2="10"/><line x1="18" y1="20" x2="18" y2="4"/><line x1="6" y1="20" x2="6" y2="16"/>'),

  sliders: sk('<line x1="4" y1="21" x2="4" y2="14"/><line x1="4" y1="10" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="12"/><line x1="12" y1="8" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="16"/><line x1="20" y1="12" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="9" y1="8" x2="15" y2="8"/><line x1="17" y1="16" x2="23" y2="16"/>'),

  'sens-low': sk('<rect x="4" y="16" width="4" height="5" rx="1"/>'),

  'sens-med': sk('<rect x="4" y="16" width="4" height="5" rx="1"/><rect x="10" y="11" width="4" height="10" rx="1"/>'),

  'sens-high': sk('<rect x="4" y="16" width="4" height="5" rx="1"/><rect x="10" y="11" width="4" height="10" rx="1"/><rect x="16" y="5" width="4" height="16" rx="1"/>'),

  medal: (c = '#e0e0e0') => `<svg ${X} viewBox="0 0 24 24" fill="none" stroke="${c}" stroke-width="2" ${R}><path d="M7.21 2L9 8.5M16.79 2L15 8.5"/><circle cx="12" cy="15" r="6"/><text x="12" y="18" text-anchor="middle" fill="${c}" stroke="none" font-size="8" font-weight="bold">1</text></svg>`,

  'medal-1': (c = '#ffd700') => `<svg ${X} viewBox="0 0 24 24" fill="none" stroke="${c}" stroke-width="2" ${R}><path d="M7.21 2L9 8.5M16.79 2L15 8.5"/><circle cx="12" cy="15" r="6"/><text x="12" y="18.5" text-anchor="middle" fill="${c}" stroke="none" font-size="9" font-weight="bold">1</text></svg>`,

  'medal-2': (c = '#c0c0c0') => `<svg ${X} viewBox="0 0 24 24" fill="none" stroke="${c}" stroke-width="2" ${R}><path d="M7.21 2L9 8.5M16.79 2L15 8.5"/><circle cx="12" cy="15" r="6"/><text x="12" y="18.5" text-anchor="middle" fill="${c}" stroke="none" font-size="9" font-weight="bold">2</text></svg>`,

  'medal-3': (c = '#cd7f32') => `<svg ${X} viewBox="0 0 24 24" fill="none" stroke="${c}" stroke-width="2" ${R}><path d="M7.21 2L9 8.5M16.79 2L15 8.5"/><circle cx="12" cy="15" r="6"/><text x="12" y="18.5" text-anchor="middle" fill="${c}" stroke="none" font-size="9" font-weight="bold">3</text></svg>`,

  dev: sk('<rect x="2" y="3" width="20" height="14" rx="2"/><line x1="8" y1="21" x2="16" y2="21"/><line x1="12" y1="17" x2="12" y2="21"/><path d="M7 8l-2 2 2 2M17 8l2 2-2 2M14 7l-4 6"/>'),

  wifi: sk('<path d="M5 12.55a11 11 0 0114.08 0"/><path d="M1.42 9a16 16 0 0121.16 0"/><path d="M8.53 16.11a6 6 0 016.95 0"/><circle cx="12" cy="20" r="1"/>'),

  upload: sk('<polyline points="16 16 12 12 8 16"/><line x1="12" y1="12" x2="12" y2="21"/><path d="M20.39 18.39A5 5 0 0018 9h-1.26A8 8 0 103 16.3"/>'),

  trash: sk('<polyline points="3 6 5 6 21 6"/><path d="M19 6v14a2 2 0 01-2 2H7a2 2 0 01-2-2V6m3 0V4a2 2 0 012-2h4a2 2 0 012 2v2"/><line x1="10" y1="11" x2="10" y2="17"/><line x1="14" y1="11" x2="14" y2="17"/>'),

  'lang': sk('<circle cx="12" cy="12" r="10"/><line x1="2" y1="12" x2="22" y2="12"/><path d="M12 2a15.3 15.3 0 014 10 15.3 15.3 0 01-4 10 15.3 15.3 0 01-4-10 15.3 15.3 0 014-10z"/>'),

  'zap-off': sk('<polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"/><line x1="1" y1="1" x2="23" y2="23"/>'),
}

export function iconUri(name, color = '#e0e0e0') {
  const fn = I[name]
  if (!fn) return ''
  const svg = typeof fn === 'function' ? fn(color) : ''
  return 'data:image/svg+xml,' + encodeURIComponent(svg)
}

export function iconNames() {
  return Object.keys(I)
}

export const KILL_CLASS_ICONS = ['fruit-fly', 'mosquito', 'fly', 'moth']

export default { iconUri, iconNames, KILL_CLASS_ICONS }
