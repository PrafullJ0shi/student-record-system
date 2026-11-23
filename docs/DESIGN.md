# Design

Visual rules for SRMS. Every figure, screen, and mark in the repository follows this plate. If a new asset needs a colour, a size, or a typeface that is not listed here, it does not belong.

## Palette

One pigment, a red-orange, on a warm neutral. The dark plate is the same pigment at a higher value so it holds on a near-black ground. No second hue, no gradients, no shadows.

| Token | Paper (light) | Plate (dark) | Use |
| --- | --- | --- | --- |
| paper | `#F3F0E8` | `#141311` | Ground |
| ink | `#1A1814` | `#E8E4DA` | Text, data marks, primary rules |
| muted | `#6E685F` | `#A39C90` | Secondary text, axis ticks |
| rule | `#C8C1B4` | `#3C3832` | Hairlines |
| accent | `#B8431A` | `#E06A3C` | One emphasis: wordmark, key, mean |

Accent on paper is `#B8431A`. Accent on the plate is `#E06A3C`. They are the same red-orange, not two brand colours.

The terminal, when colour is on, paints the paper ground and uses the paper-column values: ink `#1A1814`, muted `#6E685F`, accent `#B8431A`. It does not use the ANSI red, green, yellow, or blue slots.

## Type

Two families, and only these:

- IBM Plex Sans — prose, titles, labels, captions
- IBM Plex Mono — numerals, axes, tables, file names, code

Weights in use: Sans Regular, Sans Medium, Mono Regular, Mono Medium. No italics.

## Type scale

Seven sizes, in pixels at 1×. Raster figures are drawn at 2× and may be downsampled; the relationships stay these.

| Token | Size | Use |
| --- | --- | --- |
| meta | 11 | Running feet, axis numerals |
| caption | 13 | Figure captions, secondary labels |
| body | 15 | Body text, table cells |
| lead | 18 | Section labels inside a plate |
| title | 24 | Plate titles |
| display | 36 | Banner statement |
| banner | 48 | Wordmark |

Line height is 1.35× the size, rounded to the nearest pixel. Captions are not letterspaced. The wordmark `SRMS` is letterspaced by 0.08 em.

## Space

An eight-step scale, in pixels at 1×: 4, 8, 12, 16, 24, 32, 48, 64.

Plate margin is 32. The gap between a chart and its caption rule is 16. The gap between that rule and the caption is 12. Column gutters in a table are 16.

## Line, corner, depth

- Hairline: 1px, token `rule`, or `ink` when the line is data
- Data stroke: 1.5px, token `ink`
- Emphasis stroke: 2px, token `accent` (the mean, and the wordmark underline)
- Corner radius: 0
- Shadows, glows, blurs, gradients: none

Bars are flat rectangles. Markers are filled squares or short rules, not circles with halos.

## Figures

Each plate carries a running head in IBM Plex Mono at the caption size, left aligned:

```
Fig. N     Title
```

The explanatory sentence is set as text beside the image, not painted into the pixels. Alt text repeats that sentence.

A hairline separates the head from the drawing. Axes are labeled. Tick numerals use IBM Plex Mono. A bar chart starts at zero. The mean, when marked, is the accent; everything else is ink or muted.

Export both a paper variant and a plate variant. The README selects them with `prefers-color-scheme`. SVG text is outlined, so the drawing does not depend on fonts being installed.

## Terminal

The screen is a left-aligned column, two spaces in from the left edge, 72 characters wide. Rules use `─` (or `-` with `--ascii`). Titles are `SRMS` in accent, then the version in ink. Errors are the word `Error` in accent followed by a sentence. There is no red, no check-mark glyph, and no spinner.

## What this excludes

Inter, Roboto, Arial, Space Grotesk, and any display face outside the two families above. Purple, blue, and pink. Rounded cards. Icon sets. Emoji. Drop shadows. Multiple accent colours. All-caps eyebrows above every heading.
