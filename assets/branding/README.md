# Teleno Branding

These are the original Teleno logo and icon assets created while the native
node and desktop application still lived in the same repository. They were
recovered from `koinos/koinos-one` commit
`f752bef70f32cb076e52106002dfa37273e1326a` (`Rebrand app to Teleno and harden
producer flow`, 2026-06-09).

The SVG files are the editable sources. The PNG files are the original renders
stored in that commit, retained here to preserve the exact historical assets.

## Assets

- `logo.svg` and `logo.png`: horizontal wordmark with the original subtitle.
- `icon-mark.svg` and `icon-mark.png`: transparent standalone mark.
- `icon-dark.svg` and `icon-dark.png`: white mark on the dark app tile.
- `icon-light.svg` and `icon-light.png`: dark mark on the light app tile.
- `icon.png`: original default app icon; byte-identical to `icon-dark.png`.

## Design

The mark combines an angular peak with an open arch. Its original palette is:

- primary ink: `#071625`;
- secondary blue-gray: `#7f95aa`;
- dark icon gradient: `#14283b` to `#06111c`;
- dark-icon foreground: `#ffffff` and `#c9d7e3`;
- light-icon background and border: `#ffffff` and `#edf2f6`.

The wordmark source uses `Avenir Next`, falling back to `Helvetica Neue`,
Arial, and sans-serif. Preserve the SVG view boxes, internal spacing, stroke
proportions, and palette when producing new sizes. Prefer the SVG sources for
documentation and other resolution-independent uses.
