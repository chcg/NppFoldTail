# NppFoldTail

A Notepad++ plugin for compact folding previews.

```js
setTimeout(()=>{ … },1000);
```

```html
<h3> … </h3><p> something </p>
<!-- … -->
```

- Shows closing delimiters and hides trailing header text without changing the document.
- Click `…` to expand; hover for a preview.
- Code tooltips show the original first and last lines. HTML and comment tooltips show up to 10 lines, followed by `    ⋮` when truncated.
- Supports C/C++-lexer languages (including JavaScript), CSS, Perl, HTML, `/* */` and HTML comments.
- Hides horizontal folding lines.

## Install

Requires Windows and Notepad++ x64. x86 and ARM64 builds are not provided.

Manually tested with Notepad++ v8.9.8.1 (64-bit). Compatibility with other versions has not been verified.

Extract the **release ZIP**, then place `NppFoldTail.dll` in `Notepad++/plugins/NppFoldTail/` and restart Notepad++.
Use **Plugins → NppFoldTail → Enabled** to toggle it.

## Build

Requires x86_64 MinGW-w64 `g++` / `windres` on PATH. The original development environment uses Strawberry Perl's MinGW-w64 tools.

```powershell
.\build-x64.bat
```

Creates `build/NppFoldTail.dll`.

## Limitations

Wrapped header lines and multiline HTML opening tags are not supported. Support varies by lexer, including embedded languages in HTML. Folding previews are visual only and cannot be edited directly.

## License

[MIT](LICENSE).
