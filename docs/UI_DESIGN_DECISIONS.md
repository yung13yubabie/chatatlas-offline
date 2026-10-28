# Offline reading and graph interface

## Direction

The interface uses a reading desk with a relationship map: conversations on the left,
the current map or original messages in the centre, and contextual details beside it.
People-first indexing and a graph-only exploration console were considered; the reading
desk better balances source reading, relationship exploration and narrow screens.

The main screen is for finding and reading content, not build logs, technical reports,
fake AI activity or marketing copy. Important limitations remain available in secondary
information, rather than being hidden or presented as successful capabilities.

## Local implementation constraints

- All CSS, JavaScript and graph assets are local or embedded; no CDN, web fonts,
  analytics, online icon libraries or runtime package installation.
- Use system fonts, readable message text and clear focus/selection states.
- Retain graph positions where possible when switching the selected context.
- Motion explains a state change and settles; it is not continuous decoration.
- Honour reduced-motion preferences in both CSS and graph logic.
- Provide keyboard access and a reading alternative to pointer-based graph exploration.
- Test desktop and narrow layouts, empty states, long content, selection and dismissal.

## References reviewed

These are design references, not dependencies or installed skills.

- [Oil UI](https://github.com/oil-oil/oil-ui): compare distinct directions and refine
  real screenshots rather than choosing a generic dashboard.
- [Oil UI viewport review](https://ui.oiloil.org/guide/real-screen-blind-review/)
  and [direction comparison](https://ui.oiloil.org/guide/side-by-side-style-comparison/).
- [Beautiful UI](https://www.beautifului.dev/): contextual search, selection and source cards.
- [beUI](https://beui.dev/): segmented view switching and timeline cues, without its framework.
- [Rare UI](https://www.rareui.com/components/foldercomponent): file grouping as a secondary cue.
- [Transitions](https://transitions.dev/): explicit opacity/transform transitions and motion preferences.
- [shadcn Tabs](https://ui.shadcn.com/docs/components/base/tabs),
  [Sidebar](https://ui.shadcn.com/docs/components/base/sidebar), and
  [Sheet](https://ui.shadcn.com/docs/components/base/sheet): interaction patterns only.
- [Impeccable](https://github.com/pbakaus/impeccable): hierarchy and concise copy;
  its installation workflow and remote resources are not used.
- [UI UX Pro Max](https://github.com/nextlevelbuilder/ui-ux-pro-max-skill): resilient layouts;
  external font imports are deliberately excluded.
- [Awesome AI Tools for UI](https://github.com/maxbogo/awesome-ai-tools-for-ui): resource index,
  not evidence that an individual component has passed validation.
- [WAI tabs pattern](https://www.w3.org/WAI/ARIA/apg/patterns/tabs/),
  [interaction animation](https://www.w3.org/WAI/WCAG22/Understanding/animation-from-interactions.html),
  and [target size](https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum.html).

This design record is not a claim of a full accessibility audit or real LINE compatibility.
Actual validation is recorded separately for the exact source revision.
