// StringUtil.h — Unicode and string utility functions
#pragma once
#include "Globals.h"

namespace StringUtil {

// Strip Vietnamese diacritics and map đ/Đ to d/D (equivalent to Android's stripDiacritics)
std::wstring StripDiacritics(const std::wstring& s);

// Convert wstring to lowercase
std::wstring ToLower(const std::wstring& s);

// Split a string by a delimiter character
std::vector<std::wstring> Split(const std::wstring& s, wchar_t delimiter);

// Split a string by whitespace
std::vector<std::wstring> SplitWhitespace(const std::wstring& s);

// Join strings with a delimiter
std::wstring Join(const std::vector<std::wstring>& parts, const std::wstring& delimiter);

// Trim whitespace from both ends
std::wstring Trim(const std::wstring& s);

// Check if a character is a Vietnamese vowel (base or modified)
bool IsVowelLike(wchar_t ch);

// Check if a character is a letter
bool IsLetter(wchar_t ch);

// Read a UTF-8 file into a wstring
std::wstring ReadFileUTF8(const std::wstring& path);

// Get the installation directory (where the DLL is located)
std::wstring GetInstallDir();

// Get the user data directory (AppData\Local\NomKeyboard)
std::wstring GetUserDataDir();

// UTF-8 <-> UTF-16 conversion
std::string WideToUTF8(const std::wstring& wide);
std::wstring UTF8ToWide(const std::string& utf8);

// Levenshtein edit distance
int EditDistance(const std::wstring& a, const std::wstring& b);

} // namespace StringUtil
