# NppFoldTail

A Notepad++ plugin for compact folding previews.

```js
setTimeout(() => { … }, 1000);
function example() { … } // first comment ～ // last comment
```

```html
<section> … </section>
<!-- … -->
```

```python
def example(): …
```

## Features

- Shows closing delimiters and hides trailing header text without modifying the document.
- Click `…` to expand; hover to preview the first 10 source lines. Longer blocks end with a separate `⋮` line, approximately centred below the last displayed nonblank line.
- Optionally shows trailing comments from the first and last folded lines, using the language's single-line comment colour.
- Remembers menu settings across Notepad++ restarts.
- Highlights the preview when the hidden block is selected. Selecting through a collapsed header extends the actual selection through the hidden content for copying or deletion.
- Uses cached preview windows to avoid flicker while the caret blinks or moves.
- Hides folding lines only for supported languages; other languages retain Notepad++'s normal folding display.

## Supported languages

| Languages | Preview |
|---|---|
| C, C++, C#, Java, JavaScript, TypeScript, Go, Swift | Closing delimiters and supported block comments |
| CSS, Perl, PHP, Rust, PowerShell, R | Brace blocks; comment support varies as noted below |
| JSON, JSON5 | Objects and arrays |
| HTML, XML | Elements and comments; XML tag names are case-sensitive |
| Python | Ellipsis only, without a closing delimiter |

## Install

Requires Windows and Notepad++ x64. x86 and ARM64 builds are not provided.

Manually tested with Notepad++ v8.9.8.1 (64-bit). Compatibility with other versions has not been verified.

1. Download `NppFoldTail-0.8.0-x64.zip` from [Releases](https://github.com/seantw/NppFoldTail/releases).
2. Close all Notepad++ windows.
3. Extract the ZIP and place `NppFoldTail.dll` in `Notepad++/plugins/NppFoldTail/`.
4. Restart Notepad++ and check **Plugins → NppFoldTail → About NppFoldTail** for version **0.8.0**.

## Options

Under **Plugins → NppFoldTail**:

| Option | Default | Behaviour |
|---|---|---|
| Enable | On | Turns folding previews on or off |
| First Comment | Off | Appends a trailing comment from the folded header |
| Last Comment | Off | Appends a trailing comment from the closing line |

When both selected comments are present, they are separated by ` ～ `. No separator is added if either selected comment is missing. Comment blocks themselves are not duplicated as trailing comments. These options affect only the preview; source text and tooltip contents are unchanged.

All three settings are saved immediately to `NppFoldTail.ini` in Notepad++'s plugin configuration directory and restored on restart.

## Selecting folded content

When a single normal text selection reaches the end of a collapsed header, it extends through the complete hidden block and its closing line, excluding the following newline. The preview receives the selection background colour.

Copy and delete operate on the selected original source, including hidden content, never on the synthetic preview. A caret without a selection does not trigger this behaviour. Rectangular and multiple selections are left unchanged.

## Build

Requires x86_64 MinGW-w64 `g++` / `windres` on PATH. The development environment uses Strawberry Perl's MinGW-w64 tools.

```powershell
.\build-x64.bat
```

Creates `build/NppFoldTail.dll` using size optimisation and static C++ runtime linking.

## Limitations

- Wrapped header lines and multiline HTML/XML opening tags are not supported.
- PHP support covers brace blocks and `/* */` comments, not alternative `endif`/`endforeach` syntax.
- Other embedded script languages in HTML and XML CDATA previews are not supported.
- PowerShell block comments are not previewed.
- Synthetic preview text cannot be edited directly.

## Changes in 0.8.0

- Adds Python, JSON/JSON5, PHP, XML, Rust, PowerShell and R support.
- Unifies tooltips to the first 10 source lines.
- Adds optional first/last comment display and persistent menu settings.
- Adds whole-block selection and matching preview highlighting.
- Fixes preview flicker and hand/text cursor switching over `…`.
- Preserves native folding lines for unsupported languages.

## License

[MIT](LICENSE). Release ZIPs include the DLL and LICENSE.
