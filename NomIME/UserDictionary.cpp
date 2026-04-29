// UserDictionary.cpp — User-defined dictionary implementation
#include "UserDictionary.h"
#include "StringUtil.h"
#include "NomDictionary.h"

UserDictionary& UserDictionary::Instance() {
    static UserDictionary instance;
    return instance;
}

std::wstring UserDictionary::GetFilePath() const {
    return StringUtil::GetUserDataDir() + L"\\user_dict.tsv";
}

void UserDictionary::EnsureLoaded() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (loaded_) return;
    LoadFromFile();
    loaded_ = true;
}

void UserDictionary::LoadFromFile() {
    std::wstring path = GetFilePath();
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    std::wstring wContent = StringUtil::UTF8ToWide(content);
    std::wistringstream stream(wContent);
    std::wstring line;

    entries_.clear();
    asciiIndex_.clear();

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        line = StringUtil::Trim(line);
        if (line.empty() || line[0] == L'#') continue;

        auto parts = StringUtil::Split(line, L'\t');
        if (parts.size() < 2) continue;

        std::wstring key = StringUtil::ToLower(StringUtil::Trim(parts[0]));
        std::vector<std::wstring> values;
        for (size_t i = 1; i < parts.size(); i++) {
            auto v = StringUtil::Trim(parts[i]);
            if (!v.empty()) values.push_back(v);
        }
        if (key.empty() || values.empty()) continue;

        entries_[key] = values;
        std::wstring ascii = StringUtil::StripDiacritics(key);
        asciiIndex_[ascii].push_back(key);
    }
}

void UserDictionary::Persist() const {
    std::wstring path = GetFilePath();
    std::string utf8;
    for (auto& [k, values] : entries_) {
        utf8 += StringUtil::WideToUTF8(k);
        for (auto& v : values) {
            utf8 += "\t";
            utf8 += StringUtil::WideToUTF8(v);
        }
        utf8 += "\n";
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (file.is_open()) {
        file.write(utf8.c_str(), utf8.size());
        file.close();
    }
}

void UserDictionary::PutEntry(const std::wstring& key, const std::vector<std::wstring>& candidates) {
    std::wstring normKey = StringUtil::ToLower(StringUtil::Trim(key));
    if (normKey.empty()) return;

    std::lock_guard<std::mutex> lock(mutex_);
    // Remove old index entry
    std::wstring ascii = StringUtil::StripDiacritics(normKey);
    auto ait = asciiIndex_.find(ascii);
    if (ait != asciiIndex_.end()) {
        auto& v = ait->second;
        v.erase(std::remove(v.begin(), v.end(), normKey), v.end());
        if (v.empty()) asciiIndex_.erase(ait);
    }

    if (candidates.empty()) {
        entries_.erase(normKey);
    } else {
        entries_[normKey] = candidates;
        asciiIndex_[ascii].push_back(normKey);
    }
    Persist();
}

void UserDictionary::RemoveEntry(const std::wstring& key) {
    PutEntry(key, {});
}

std::vector<std::wstring> UserDictionary::LookupSingle(const std::wstring& query, bool strict) const {
    if (query.empty()) return {};
    std::wstring q = StringUtil::ToLower(query);
    std::vector<std::wstring> result;

    auto it = entries_.find(q);
    if (it != entries_.end() && q.find(L' ') == std::wstring::npos) {
        result.insert(result.end(), it->second.begin(), it->second.end());
    }
    if (strict) return result;

    std::wstring qAscii = StringUtil::StripDiacritics(q);
    auto ait = asciiIndex_.find(qAscii);
    if (ait != asciiIndex_.end()) {
        for (auto& k : ait->second) {
            if (k.find(L' ') != std::wstring::npos) continue;
            auto eit = entries_.find(k);
            if (eit == entries_.end()) continue;
            for (auto& v : eit->second) {
                bool found = false;
                for (auto& r : result) if (r == v) { found = true; break; }
                if (!found) result.push_back(v);
            }
        }
    }
    return result;
}

std::vector<std::wstring> UserDictionary::LookupWord(const std::wstring& query, bool strict) const {
    if (query.empty()) return {};
    std::wstring q = StringUtil::ToLower(query);
    std::vector<std::wstring> result;

    auto it = entries_.find(q);
    if (it != entries_.end()) {
        result.insert(result.end(), it->second.begin(), it->second.end());
    }
    if (strict) return result;

    std::wstring qAscii = StringUtil::StripDiacritics(q);
    auto ait = asciiIndex_.find(qAscii);
    if (ait != asciiIndex_.end()) {
        for (auto& k : ait->second) {
            auto eit = entries_.find(k);
            if (eit == entries_.end()) continue;
            for (auto& v : eit->second) {
                bool found = false;
                for (auto& r : result) if (r == v) { found = true; break; }
                if (!found) result.push_back(v);
            }
        }
    }
    return result;
}

std::vector<std::wstring> UserDictionary::LookupPrefix(const std::wstring& query, int limit) const {
    if (query.empty()) return {};
    std::wstring qAscii = StringUtil::StripDiacritics(StringUtil::ToLower(query));
    qAscii.erase(std::remove(qAscii.begin(), qAscii.end(), L' '), qAscii.end());
    if (qAscii.empty()) return {};

    std::vector<std::wstring> result;
    for (auto& [asciiKey, originals] : asciiIndex_) {
        if ((int)result.size() >= limit) break;
        std::wstring compact = asciiKey;
        compact.erase(std::remove(compact.begin(), compact.end(), L' '), compact.end());
        if (compact.size() > qAscii.size() && compact.substr(0, qAscii.size()) == qAscii) {
            for (auto& orig : originals) {
                auto eit = entries_.find(orig);
                if (eit == entries_.end()) continue;
                for (auto& v : eit->second) {
                    bool found = false;
                    for (auto& r : result) if (r == v) { found = true; break; }
                    if (!found) { result.push_back(v); if ((int)result.size() >= limit) break; }
                }
                if ((int)result.size() >= limit) break;
            }
        }
    }
    return result;
}

std::vector<std::pair<std::wstring, std::vector<std::wstring>>>
UserDictionary::LookupWordByVietTat(const std::vector<std::wstring>& segments, int limit) const {
    if (segments.size() < 2) return {};
    int n = (int)segments.size();

    std::vector<std::pair<std::wstring, std::vector<std::wstring>>> result;
    for (auto& [key, values] : entries_) {
        if ((int)result.size() >= limit) break;
        if (key.find(L' ') == std::wstring::npos) continue;
        auto sylls = StringUtil::SplitWhitespace(key);
        if ((int)sylls.size() != n) continue;

        bool ok = true;
        for (int i = 0; i < n; i++) {
            auto ascii = StringUtil::StripDiacritics(sylls[i]);
            if (ascii.size() < segments[i].size() ||
                ascii.substr(0, segments[i].size()) != segments[i]) {
                ok = false; break;
            }
        }
        if (!ok) continue;
        result.push_back({ key, values });
    }
    return result;
}

int UserDictionary::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return (int)entries_.size();
}
