# Nom Keyboard (Bàn phím Hán Nôm)

A Hán Nôm input method for Windows 10/11, using Telex Vietnamese input.

[Tiếng Việt](README.md)

## Features / Tính năng

- **Telex input**: aa→â, aw→ă, ee→ê, oo→ô, ow→ơ, uw→ư, dd→đ; tone marks via s/f/r/x/j/z
- **Segment mode** (phân đoạn): type multiple syllables, pick compounds from the candidate bar
- **Viết tắt** (abbreviated input): type initials like "qg" to get "quốc gia" → 國家
- **User dictionary**: auto-learns your picks, importable/exportable as TSV
- **Win10/11 style UI**: candidate window follows the modern Microsoft Pinyin design
- **Old/New tone style**: configurable traditional (hóa) vs modern (hoá) tone placement
- **No bundled fonts**: relies on system CJK fonts (SimSun-ExtB, MingLiU-ExtB, etc.)

## Building / Biên dịch

### Prerequisites / Yêu cầu

1. **Visual Studio 2022** (v17+) with "Desktop development with C++" workload
2. **Windows SDK 10.0** (included with VS2022)
3. **NSIS 3.x** (https://nsis.sourceforge.io/) — for building setup.exe
   - After installing, add NSIS to your PATH

### Build Steps / Các bước

1. Open `NomIME.sln` in Visual Studio 2022
2. Select **Release | x64** configuration
3. Build the solution (Ctrl+Shift+B):
   - First `NomIME` compiles → `bin\Release\x64\NomIME.dll`
   - Also build **Release | Win32** for the 32-bit DLL
   - Then `NomIMESetup` invokes NSIS → `NomIMESetup\setup.exe`

### Manual Registration (for development)

Instead of running the installer, you can register the DLL directly:

```cmd
:: Register (run as Administrator)
regsvr32 bin\Release\x64\NomIME.dll

:: Unregister
regsvr32 /u bin\Release\x64\NomIME.dll
```

After registration, go to **Settings → Time & Language → Language → Vietnamese → Options → Add an input method** and select "Nom Keyboard (Bàn phím Hán Nôm)".

## Usage / Cách sử dụng

1. Switch to Nom Keyboard using **Win+Space** or the language bar
2. Type Vietnamese in Telex: e.g. `quocs` → `quốc`
3. Candidates appear in a popup; press **1-9** to pick, or click
4. **Space** adds a syllable separator (segment mode); double-space commits
5. **Enter** commits the raw Vietnamese text
6. **Escape** cancels the composition
7. **Backspace** deletes characters one by one, undoes locked picks

### Viết tắt (Abbreviated Input)

Type consonant clusters like `qg` to match "quốc gia" → 國家. The engine
automatically detects when your input can't be a valid Vietnamese syllable
and splits it into prefix segments.

## Dictionary Data / Dữ liệu từ điển

The dictionary files are plain UTF-8 TSV (tab-separated values):

- `nom_dict_single.tsv`: `<Vietnamese syllable>\t<Nom char 1>\t<Nom char 2>\t...`
- `nom_dict_word.tsv`: `<Vietnamese compound>\t<Nom word 1>\t<Nom word 2>\t...`

User dictionary is stored at `%LOCALAPPDATA%\NomKeyboard\user_dict.tsv` in the
same format. You can edit it by hand or import/export in a future settings UI.

## Configuration / Cài đặt

Preferences are stored in the registry at `HKCU\Software\NomKeyboard`:

| Value | Type | Default | Description |
|-------|------|---------|-------------|
| `ToneStyleOld` | DWORD | 0 | 1=traditional (hóa), 0=modern (hoá) |
| `ShorthandEnabled` | DWORD | 1 | 1=enable viết tắt, 0=disable |
| `SegmentMode` | DWORD | 1 | 1=space as syllable separator, 0=space commits |

## License

Same license as the Android counterpart. See the project root for details.
