# Native file associations

Run `adobewine launchers` after updating LinDobe to refresh file associations and
application-menu entries. It needs `shared-mime-info` (`update-mime-database`)
and `desktop-file-utils` (`update-desktop-database`).

File-manager opening has two parts: identifying the file's MIME type, and finding
an installed application whose desktop entry supports that type. Previously,
After Effects had an empty `MimeType` entry. InDesign listed
`application/x-indesign`, but LinDobe supplied no rules recognizing its extensions.
The same missing definitions affected other native project and session formats.

`config/adobewine.xml` supplies the extension rules; `config/apps.txt` declares
the handlers. The launcher installer registers the XML in
`$XDG_DATA_HOME/mime/packages/adobewine.xml` (normally
`~/.local/share/mime/packages/adobewine.xml`) and refreshes both databases.
Uppercase extensions work too. Entries are generated only for installed apps.

New types declare themed generic icons: video for After Effects, Premiere and
Animate, audio for Audition, documents for InDesign and InCopy, and images for
Lightroom Classic. A theme's format-specific icon takes precedence when available.
The desktop's icon theme supplies the artwork and colors; no Adobe icons are bundled.

| Application | Native extensions |
| --- | --- |
| Photoshop | `.psd`, `.psb`, `.psdt` |
| Illustrator | `.ai`, `.ait` |
| After Effects | `.aep`, `.aepx`, `.aet` |
| Premiere Pro | `.prproj` |
| Audition | `.sesx` |
| InDesign | `.indd`, `.indt`, `.indb`, `.indl`, `.idml`, `.idap` |
| InCopy | `.icml`, `.icmt`, `.icma`, `.icap` |
| Animate | `.fla`, `.xfl` |
| Lightroom Classic | `.lrcat` |

The existing image, audio, PostScript, SVG and PDF handler declarations are
unchanged. Importable media, presets, scripts, lock files and catalog sidecars
are not added as native documents. Apps without a native document format, such
as Bridge, do not claim other apps' projects.

Registration does not write `mimeapps.list` or change an existing default
application. The desktop can select a registered handler when there is no
explicit preference. If necessary, choose the application with the file
manager's **Open With** command. Do not associate a generic type such as
`text/plain` or `application/zip` with an Adobe app to open its projects: that
would also affect unrelated text files or archives.

`adobewine launchers --remove` removes LinDobe's desktop entries and XML, then
refreshes both databases. It preserves other MIME packages and user preferences.

## Verification

Run `python3 -B -m unittest discover -s tests/launcher -v`. The suite checks MIME
recognition and GIO file dispatch to stub Wine in disposable XDG directories,
including uppercase names, paths with spaces, the configured Flatpak command,
idempotence, uninstalled apps, existing preferences and removal. These tests do
not start Adobe apps or a Flatpak sandbox and do not verify project parsing in
Adobe. No Wine rebuild is needed for this change.

The registration follows the [Shared MIME-info specification](https://specifications.freedesktop.org/shared-mime-info/latest/ar01s02.html)
and [MIME application default selection](https://specifications.freedesktop.org/mime-apps/latest/default).
Format references: [After Effects projects](https://helpx.adobe.com/after-effects/using/projects.html),
[InDesign formats](https://helpx.adobe.com/indesign/desktop/get-started/system-and-product-info/supported-file-formats.html),
[Audition sessions](https://helpx.adobe.com/audition/desktop/importing-recording-and-playing/creating-opening-files.html),
[InCopy packages](https://helpx.adobe.com/incopy/desktop/work-with-indesign/managed-files/assignment-packages.html),
[Animate documents](https://helpx.adobe.com/ph_fil/animate/desktop/introduction-to-animate/documents.html),
and [Lightroom Classic catalogs](https://helpx.adobe.com/lightroom-classic/desktop/manage-catalogs-and-files/create-catalogs.html).
