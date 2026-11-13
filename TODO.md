# TODO

- [x] Audio book, keep code but deactivate plugin
- [x] Dimensions, remove the plugin
- [x] OutputPreview, remove the plugin
- [x] Redact, keep the code but deactivate the plugin
- [x] SoftProofing, keep code but deactivate the plugin
- [x] About dialog: show the inkpage name and the GPLv3 licence (it still shows the PDF4QT MIT text)

## Sidebar (reference)

The sidebar has 7 buttons (`Pdf4QtLibGui/pdfsidebarwidget.*`); a button is disabled when its page has nothing to show.

| # | Button | Icon (`Pdf4QtLibGui/resources/`) | Function | Disabled when |
|---|---|---|---|---|
| 1 | Outline | `sidebar-outline.svg` | Table of contents of the PDF (PDF bookmarks): click to navigate, search field. Editable in the editor (drag and drop, context menu: Follow, New Outline Item, Delete, Rename, Move Up/Down/Left/Right, Font Bold/Italic) | never (editable) |
| 2 | Thumbnails | `sidebar-thumbnails.svg` | Page thumbnails: click to go to a page, size slider, synchronize button (`synchronize.svg`) | no page |
| 3 | Visibility | `sidebar-visibility.svg` | Optional content (layers): show/hide the PDF layers | no layers |
| 4 | Attachments | `sidebar-attachment.svg` | Embedded files: double-click to open, context menu Open Attachment / Save to File... | no attachments |
| 5 | Signatures | `sidebar-signature.svg` | Digital signatures and timestamps: validation result, certificate details, certificate chain | not signed |
| 6 | Bookmarks | `sidebar-favourites.svg` | User bookmarks stored by inkpage (not in the PDF): Bookmark Page (Ctrl+M), next/previous, generate automatically, import/export | no document |
| 7 | Notes | `sidebar-annotations.svg` | List of annotations by page and author, search field, click to go to a note, selection synchronized with the page | no annotations |

The PDF can choose the page shown at opening (outline, thumbnails, layers, attachments). Button icon size: application settings (small/medium/large).
Candidates to hide for a notebook-style editor: Visibility, Attachments, Signatures.
