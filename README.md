# Bàn phím Hán Nôm cho Windows

[English](README.en.md)

Bộ gõ Telex cho Windows 10/11, hỗ trợ từ ghép, viết tắt và từ điển cá nhân. Cửa sổ ứng viên dùng phông Minh Nguyên và tự bổ sung nét chữ còn thiếu cho toàn bộ từ điển đi kèm.

## Cài đặt và sử dụng

1. Tải và chạy **NomKeyboardSetup.exe** từ [GitHub Releases](https://github.com/CjangCjengh/nom_keyboard_windows/releases/latest). Bộ cài đăng ký cả DLL 32-bit và 64-bit trên Windows x64.
2. Nhấn **Win+Space** để chuyển sang Bàn phím Hán Nôm. Nếu chưa thấy, thêm bộ gõ trong tuỳ chọn ngôn ngữ tiếng Việt của Windows.
3. Gõ tiếng Việt bằng Telex. Nhấn **1–9** hoặc nhấp chuột để chọn chữ, dùng **Page Up / Page Down** để chuyển trang. Trong chế độ phân đoạn, **Space** tách âm tiết và **Enter** xác nhận phần đang gõ.

Mở **Nom Keyboard Settings** từ menu Start để đổi kiểu đặt dấu, viết tắt và chế độ phân đoạn. Từ điển cá nhân nằm tại `%LOCALAPPDATA%\NomKeyboard\user_dict.tsv`.

Để gỡ cài đặt, mở danh sách ứng dụng đã cài trong Windows. Nếu DLL đang được dùng, chuyển sang bàn phím khác, lưu công việc, đóng ứng dụng được liệt kê rồi chọn **Retry**.

## Biên dịch

Mở `NomIME.sln` trong Visual Studio 2022 trở lên với workload **Desktop development with C++**, Windows SDK và NSIS 3. Biên dịch **Release | x64** để tạo hai DLL và `NomIMESetup\NomKeyboardSetup.exe`. [Giấy phép và nguồn phông chữ](data/CandidateFont-LICENSE.txt)
