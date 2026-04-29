// StringUtil.cpp — Unicode and string utility functions
#include "StringUtil.h"
#include <locale>
#include <codecvt>
#include <shlobj.h>

namespace StringUtil {

// Vietnamese character decomposition table: toned/modified -> base
// This covers all Vietnamese vowels with diacritics
static const std::unordered_map<wchar_t, wchar_t>& GetDecompMap() {
    static std::unordered_map<wchar_t, wchar_t> map = {
        // a family
        {L'á',L'a'},{L'à',L'a'},{L'ả',L'a'},{L'ã',L'a'},{L'ạ',L'a'},
        {L'ă',L'a'},{L'ắ',L'a'},{L'ằ',L'a'},{L'ẳ',L'a'},{L'ẵ',L'a'},{L'ặ',L'a'},
        {L'â',L'a'},{L'ấ',L'a'},{L'ầ',L'a'},{L'ẩ',L'a'},{L'ẫ',L'a'},{L'ậ',L'a'},
        {L'Á',L'A'},{L'À',L'A'},{L'Ả',L'A'},{L'Ã',L'A'},{L'Ạ',L'A'},
        {L'Ă',L'A'},{L'Ắ',L'A'},{L'Ằ',L'A'},{L'Ẳ',L'A'},{L'Ẵ',L'A'},{L'Ặ',L'A'},
        {L'Â',L'A'},{L'Ấ',L'A'},{L'Ầ',L'A'},{L'Ẩ',L'A'},{L'Ẫ',L'A'},{L'Ậ',L'A'},
        // e family
        {L'é',L'e'},{L'è',L'e'},{L'ẻ',L'e'},{L'ẽ',L'e'},{L'ẹ',L'e'},
        {L'ê',L'e'},{L'ế',L'e'},{L'ề',L'e'},{L'ể',L'e'},{L'ễ',L'e'},{L'ệ',L'e'},
        {L'É',L'E'},{L'È',L'E'},{L'Ẻ',L'E'},{L'Ẽ',L'E'},{L'Ẹ',L'E'},
        {L'Ê',L'E'},{L'Ế',L'E'},{L'Ề',L'E'},{L'Ể',L'E'},{L'Ễ',L'E'},{L'Ệ',L'E'},
        // i family
        {L'í',L'i'},{L'ì',L'i'},{L'ỉ',L'i'},{L'ĩ',L'i'},{L'ị',L'i'},
        {L'Í',L'I'},{L'Ì',L'I'},{L'Ỉ',L'I'},{L'Ĩ',L'I'},{L'Ị',L'I'},
        // o family
        {L'ó',L'o'},{L'ò',L'o'},{L'ỏ',L'o'},{L'õ',L'o'},{L'ọ',L'o'},
        {L'ô',L'o'},{L'ố',L'o'},{L'ồ',L'o'},{L'ổ',L'o'},{L'ỗ',L'o'},{L'ộ',L'o'},
        {L'ơ',L'o'},{L'ớ',L'o'},{L'ờ',L'o'},{L'ở',L'o'},{L'ỡ',L'o'},{L'ợ',L'o'},
        {L'Ó',L'O'},{L'Ò',L'O'},{L'Ỏ',L'O'},{L'Õ',L'O'},{L'Ọ',L'O'},
        {L'Ô',L'O'},{L'Ố',L'O'},{L'Ồ',L'O'},{L'Ổ',L'O'},{L'Ỗ',L'O'},{L'Ộ',L'O'},
        {L'Ơ',L'O'},{L'Ớ',L'O'},{L'Ờ',L'O'},{L'Ở',L'O'},{L'Ỡ',L'O'},{L'Ợ',L'O'},
        // u family
        {L'ú',L'u'},{L'ù',L'u'},{L'ủ',L'u'},{L'ũ',L'u'},{L'ụ',L'u'},
        {L'ư',L'u'},{L'ứ',L'u'},{L'ừ',L'u'},{L'ử',L'u'},{L'ữ',L'u'},{L'ự',L'u'},
        {L'Ú',L'U'},{L'Ù',L'U'},{L'Ủ',L'U'},{L'Ũ',L'U'},{L'Ụ',L'U'},
        {L'Ư',L'U'},{L'Ứ',L'U'},{L'Ừ',L'U'},{L'Ử',L'U'},{L'Ữ',L'U'},{L'Ự',L'U'},
        // y family
        {L'ý',L'y'},{L'ỳ',L'y'},{L'ỷ',L'y'},{L'ỹ',L'y'},{L'ỵ',L'y'},
        {L'Ý',L'Y'},{L'Ỳ',L'Y'},{L'Ỷ',L'Y'},{L'Ỹ',L'Y'},{L'Ỵ',L'Y'},
        // d
        {L'đ',L'd'},{L'Đ',L'D'},
    };
    return map;
}

std::wstring StripDiacritics(const std::wstring& s) {
    if (s.empty()) return s;
    const auto& decomp = GetDecompMap();
    std::wstring result;
    result.reserve(s.size());
    for (wchar_t ch : s) {
        auto it = decomp.find(ch);
        if (it != decomp.end()) {
            result.push_back(it->second);
        } else {
            result.push_back(ch);
        }
    }
    return result;
}

std::wstring ToLower(const std::wstring& s) {
    std::wstring result = s;
    for (auto& ch : result) {
        ch = towlower(ch);
    }
    return result;
}

std::vector<std::wstring> Split(const std::wstring& s, wchar_t delimiter) {
    std::vector<std::wstring> tokens;
    std::wistringstream stream(s);
    std::wstring token;
    while (std::getline(stream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<std::wstring> SplitWhitespace(const std::wstring& s) {
    std::vector<std::wstring> tokens;
    std::wistringstream stream(s);
    std::wstring token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

std::wstring Join(const std::vector<std::wstring>& parts, const std::wstring& delimiter) {
    std::wstring result;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0) result += delimiter;
        result += parts[i];
    }
    return result;
}

std::wstring Trim(const std::wstring& s) {
    auto start = s.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";
    auto end = s.find_last_not_of(L" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool IsVowelLike(wchar_t ch) {
    // Base vowels + all modified forms
    static const std::wstring vowels = L"aAăĂâÂeEêÊiIoOôÔơƠuUưƯyY"
        L"áàảãạắằẳẵặấầẩẫậ"
        L"ÁÀẢÃẠẮẰẲẴẶẤẦẨẪẬ"
        L"éèẻẽẹếềểễệ"
        L"ÉÈẺẼẸẾỀỂỄỆ"
        L"íìỉĩị"
        L"ÍÌỈĨỊ"
        L"óòỏõọốồổỗộớờởỡợ"
        L"ÓÒỎÕỌỐỒỔỖỘỚỜỞỠỢ"
        L"úùủũụứừửữự"
        L"ÚÙỦŨỤỨỪỬỮỰ"
        L"ýỳỷỹỵ"
        L"ÝỲỶỸỴ";
    return vowels.find(ch) != std::wstring::npos;
}

bool IsLetter(wchar_t ch) {
    return iswalpha(ch) != 0;
}

std::wstring GetInstallDir() {
    wchar_t path[MAX_PATH] = { 0 };
    if (g_hInst) {
        GetModuleFileNameW(g_hInst, path, MAX_PATH);
        std::wstring dir(path);
        auto pos = dir.find_last_of(L'\\');
        if (pos != std::wstring::npos) {
            return dir.substr(0, pos);
        }
    }
    return L".";
}

std::wstring GetUserDataDir() {
    wchar_t* appdata = nullptr;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &appdata))) {
        dir = std::wstring(appdata) + L"\\NomKeyboard";
        CoTaskMemFree(appdata);
    } else {
        dir = GetInstallDir() + L"\\UserData";
    }
    CreateDirectoryW(dir.c_str(), NULL);
    return dir;
}

std::string WideToUTF8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &result[0], size, nullptr, nullptr);
    return result;
}

std::wstring UTF8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &result[0], size);
    return result;
}

int EditDistance(const std::wstring& a, const std::wstring& b) {
    if (a == b) return 0;
    if (a.empty()) return (int)b.size();
    if (b.empty()) return (int)a.size();
    int m = (int)a.size(), n = (int)b.size();
    std::vector<int> prev(n + 1), curr(n + 1);
    for (int j = 0; j <= n; j++) prev[j] = j;
    for (int i = 1; i <= m; i++) {
        curr[0] = i;
        for (int j = 1; j <= n; j++) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            curr[j] = (std::min)({ prev[j] + 1, curr[j-1] + 1, prev[j-1] + cost });
        }
        std::swap(prev, curr);
    }
    return prev[n];
}

} // namespace StringUtil
