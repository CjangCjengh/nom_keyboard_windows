# Bàn phím Hán Nôm (Nom Keyboard)

Bàn phím Hán Nôm cho Windows 10/11, sử dụng kiểu gõ Telex.

[English](README.en.md)

## Tính năng

- **Gõ Telex**: aa→â, aw→ă, ee→ê, oo→ô, ow→ơ, uw→ư, dd→đ; dấu thanh: s/f/r/x/j/z
- **Chế độ phân đoạn**: gõ nhiều âm tiết, chọn từ ghép từ thanh ứng viên
- **Viết tắt**: gõ chữ cái đầu như "qg" để tra "quốc gia" → 國家
- **Từ điển người dùng**: tự động ghi nhớ, xuất/nhập dưới dạng TSV
- **Giao diện Win10/11**: cửa sổ ứng viên theo phong cách Microsoft Pinyin
- **Kiểu đặt dấu**: hỗ trợ cả kiểu cũ (hóa) và kiểu mới (hoá)
- **Không cần font đi kèm**: sử dụng font CJK hệ thống

## Kiến trúc

```
NomIME.sln
├── NomIME/                    # DLL bộ gõ (TSF Text Service Framework)
│   ├── DllMain.cpp            # Điểm vào DLL + xuất COM
│   ├── Register.cpp           # Đăng ký COM server + TSF profile
│   ├── ClassFactory.cpp       # COM class factory
│   ├── TextService.cpp/.h     # Triển khai ITfTextInputProcessorEx chính
│   ├── KeyEventSink.cpp       # Xử lý sự kiện phím (ITfKeyEventSink)
│   ├── Composition.cpp        # Quản lý composition TSF
│   ├── CandidateWindow.cpp/.h # Cửa sổ chọn ứng viên (phong cách Win10/11)
│   ├── TelexEngine.cpp/.h     # Bộ máy gõ Telex tiếng Việt
│   ├── NomDictionary.cpp/.h   # Từ điển đi kèm (dạng TSV)
│   ├── UserDictionary.cpp/.h  # Từ điển người dùng (tự học)
│   ├── StringUtil.cpp/.h      # Tiện ích Unicode (dấu, UTF-8/16, v.v.)
│   ├── Globals.h              # GUID, hằng số, include chung
│   ├── NomIME.def             # Xuất DLL
│   └── NomIME.rc              # Tài nguyên phiên bản
├── NomIMESetup/               # Dự án trình cài đặt NSIS
│   └── NomIMESetup.nsi        # Script NSIS → setup.exe
└── data/                      # Dữ liệu từ điển
    ├── nom_dict_single.tsv    # 6.686 âm tiết → 25.059 chữ Nôm
    └── nom_dict_word.tsv      # 2.648 từ ghép
```

## Biên dịch

### Yêu cầu

1. **Visual Studio 2022** với workload "Desktop development with C++"
2. **Windows SDK 10.0**
3. **NSIS 3.x** (https://nsis.sourceforge.io/) — để tạo setup.exe

### Các bước

1. Mở `NomIME.sln` trong Visual Studio 2022
2. Chọn cấu hình **Release | x64**, build solution (Ctrl+Shift+B)
3. Đổi sang **Release | Win32** và build lại
4. Build project `NomIMESetup` → `NomIMESetup\setup.exe`

### Đăng ký thủ công (khi phát triển)

Thay vì chạy trình cài đặt, bạn có thể đăng ký DLL trực tiếp:

```cmd
:: Đăng ký (chạy với quyền Administrator)
regsvr32 bin\Release\x64\NomIME.dll

:: Hủy đăng ký
regsvr32 /u bin\Release\x64\NomIME.dll
```

Sau khi đăng ký, vào **Settings → Time & Language → Language → Vietnamese → Options → Add an input method** và chọn "Bàn phím Hán Nôm (Nom Keyboard)".

## Cách sử dụng

1. Chạy `setup.exe` để cài đặt
2. Chuyển sang Bàn phím Hán Nôm bằng **Win+Space** hoặc thanh ngôn ngữ
3. Gõ tiếng Việt Telex, ví dụ: `quocs` → `quốc`
4. Ứng viên hiện ra; nhấn **1-9** để chọn, hoặc nhấp chuột
5. **Space** thêm dấu phân âm tiết (chế độ phân đoạn); gõ Space hai lần để kết thúc
6. **Enter** xác nhận văn bản tiếng Việt thô
7. **Escape** hủy bỏ
8. **Backspace** xóa từng ký tự, hoàn tác lựa chọn đã khóa

### Viết tắt

Gõ cụm phụ âm như `qg` để khớp "quốc gia" → 國家. Bộ máy tự động
phát hiện khi đầu vào không thể là một âm tiết tiếng Việt hợp lệ và
tách thành các đoạn tiền tố.

## Dữ liệu từ điển

Các tệp từ điển là TSV (phân tách bằng tab) mã hóa UTF-8:

- `nom_dict_single.tsv`: `<âm tiết tiếng Việt>\t<chữ Nôm 1>\t<chữ Nôm 2>\t...`
- `nom_dict_word.tsv`: `<từ ghép tiếng Việt>\t<từ Nôm 1>\t<từ Nôm 2>\t...`

Từ điển người dùng lưu tại `%LOCALAPPDATA%\NomKeyboard\user_dict.tsv` cùng định dạng.

## Cài đặt tùy chọn

Tùy chọn lưu trong registry tại `HKCU\Software\NomKeyboard`:

| Giá trị | Kiểu | Mặc định | Mô tả |
|---------|------|----------|-------|
| `ToneStyleOld` | DWORD | 0 | 1=kiểu cũ (hóa), 0=kiểu mới (hoá) |
| `ShorthandEnabled` | DWORD | 1 | 1=bật viết tắt, 0=tắt |
| `SegmentMode` | DWORD | 1 | 1=space phân âm tiết, 0=space xác nhận |

## Giấy phép

Cùng giấy phép với phiên bản Android. Xem thư mục gốc dự án để biết chi tiết.
