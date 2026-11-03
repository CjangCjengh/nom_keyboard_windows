# Nom Keyboard for Windows

[Tiếng Việt](README.md)

A Vietnamese Telex input method for Hán Nôm on Windows 10/11, with compound words, abbreviated input and a personal dictionary. The candidate window uses Minh Nguyên glyphs with coverage for every character in the bundled dictionaries.

## Install and type

1. Download and run **NomKeyboardSetup.exe** from [GitHub Releases](https://github.com/CjangCjengh/nom_keyboard_windows/releases/latest). On x64 Windows, setup registers both the 32-bit and 64-bit input services.
2. Press **Win+Space** to switch to Nom Keyboard. If it is missing, add it under the Vietnamese language options in Windows settings.
3. Type Vietnamese with Telex. Press **1–9** or click to pick a character. Use **Page Up / Page Down** to browse candidates. In segment mode, **Space** separates syllables and **Enter** commits the current text.

Open **Nom Keyboard Settings** from Start to change tone placement, abbreviated input or segment mode. The personal dictionary is stored at `%LOCALAPPDATA%\NomKeyboard\user_dict.tsv`.

Uninstall from the Windows installed apps list. If a DLL is in use, switch keyboards, save your work, close the listed apps and choose **Retry**.

## Build

Open `NomIME.sln` in Visual Studio 2022 or later with **Desktop development with C++**, the Windows SDK and NSIS 3 installed. Build **Release | x64** to produce both DLLs and `NomIMESetup\NomKeyboardSetup.exe`. [Font sources and license notices](data/CandidateFont-LICENSE.txt)
