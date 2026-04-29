// NomDictionary.cpp — Nom character dictionary implementation
#include "NomDictionary.h"
#include "StringUtil.h"

NomDictionary& NomDictionary::Instance() {
    static NomDictionary instance;
    return instance;
}

std::wstring NomDictionary::StripDiacritics(const std::wstring& s) {
    return StringUtil::StripDiacritics(s);
}

void NomDictionary::EnsureLoaded(const std::wstring& dataDir) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (loaded_) return;

    LoadTsv(dataDir + L"\\nom_dict_single.tsv", singleMap_, asciiSingleIndex_);
    LoadTsv(dataDir + L"\\nom_dict_word.tsv", wordMap_, asciiWordIndex_);
    BuildVietTatIndex();
    BuildNomReverseIndex();
    loaded_ = true;
}

void NomDictionary::LoadTsv(const std::wstring& path,
    std::unordered_map<std::wstring, std::vector<std::wstring>>& target,
    std::unordered_map<std::wstring, std::vector<std::wstring>>& asciiIdx)
{
    // Read file as UTF-8
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    std::wstring wContent = StringUtil::UTF8ToWide(content);
    std::wistringstream stream(wContent);
    std::wstring line;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        // Remove trailing \r
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty()) continue;

        auto parts = StringUtil::Split(line, L'\t');
        if (parts.size() < 2) continue;

        std::wstring key = parts[0];
        std::vector<std::wstring> values;
        for (size_t i = 1; i < parts.size(); i++) {
            if (!parts[i].empty()) values.push_back(parts[i]);
        }
        if (values.empty()) continue;

        target[key] = values;
        std::wstring ascii = StringUtil::StripDiacritics(key);
        asciiIdx[ascii].push_back(key);
    }
}

void NomDictionary::BuildVietTatIndex() {
    for (auto& [origKey, _] : wordMap_) {
        if (origKey.find(L' ') == std::wstring::npos) continue;
        auto sylls = StringUtil::SplitWhitespace(origKey);
        if (sylls.size() < 2) continue;

        std::vector<std::wstring> asciiSylls;
        bool valid = true;
        for (auto& s : sylls) {
            auto ascii = StringUtil::StripDiacritics(StringUtil::ToLower(s));
            if (ascii.empty()) { valid = false; break; }
            asciiSylls.push_back(ascii);
        }
        if (!valid) continue;

        wordSyllablesAscii_[origKey] = asciiSylls;
        wordsBySyllableCount_[(int)asciiSylls.size()].push_back(origKey);

        std::wstring bucketKey = std::to_wstring(asciiSylls.size()) + L"|" + asciiSylls[0].substr(0, 1);
        wordsBySyllableCountAndFirstChar_[bucketKey].push_back(origKey);
    }
}

void NomDictionary::BuildNomReverseIndex() {
    for (auto& [rawReading, noms] : singleMap_) {
        std::wstring reading = StringUtil::ToLower(StringUtil::Trim(rawReading));
        if (reading.empty()) continue;

        for (auto& nom : noms) {
            auto& readings = nomToSingleReadings_[nom];
            // Avoid duplicates
            bool found = false;
            for (auto& r : readings) {
                if (r == reading) { found = true; break; }
            }
            if (!found) readings.push_back(reading);
        }
    }
}

std::vector<std::wstring> NomDictionary::LookupSingle(const std::wstring& query, bool strict) const {
    if (query.empty()) return {};
    std::wstring q = StringUtil::ToLower(query);
    std::vector<std::wstring> result;

    auto it = singleMap_.find(q);
    if (it != singleMap_.end()) {
        result.insert(result.end(), it->second.begin(), it->second.end());
    }
    if (strict) return result;

    std::wstring qAscii = StringUtil::StripDiacritics(q);
    auto ait = asciiSingleIndex_.find(qAscii);
    if (ait != asciiSingleIndex_.end()) {
        for (auto& k : ait->second) {
            auto sit = singleMap_.find(k);
            if (sit != singleMap_.end()) {
                for (auto& v : sit->second) {
                    // Deduplicate
                    bool found = false;
                    for (auto& r : result) if (r == v) { found = true; break; }
                    if (!found) result.push_back(v);
                }
            }
        }
    }
    return result;
}

std::vector<std::wstring> NomDictionary::LookupWord(const std::wstring& query, bool strict) const {
    if (query.empty()) return {};
    std::wstring q = StringUtil::ToLower(query);
    std::vector<std::wstring> result;

    auto it = wordMap_.find(q);
    if (it != wordMap_.end()) {
        result.insert(result.end(), it->second.begin(), it->second.end());
    }
    // Also try without spaces
    std::wstring qNoSp = q;
    qNoSp.erase(std::remove(qNoSp.begin(), qNoSp.end(), L' '), qNoSp.end());
    if (qNoSp != q) {
        auto it2 = wordMap_.find(qNoSp);
        if (it2 != wordMap_.end()) {
            for (auto& v : it2->second) {
                bool found = false;
                for (auto& r : result) if (r == v) { found = true; break; }
                if (!found) result.push_back(v);
            }
        }
    }
    if (strict) return result;

    std::wstring qAscii = StringUtil::StripDiacritics(q);
    auto ait = asciiWordIndex_.find(qAscii);
    if (ait != asciiWordIndex_.end()) {
        for (auto& k : ait->second) {
            auto wit = wordMap_.find(k);
            if (wit != wordMap_.end()) {
                for (auto& v : wit->second) {
                    bool found = false;
                    for (auto& r : result) if (r == v) { found = true; break; }
                    if (!found) result.push_back(v);
                }
            }
        }
    }
    return result;
}

std::vector<std::wstring> NomDictionary::LookupSinglePrefix(const std::wstring& prefix, int limit, bool strict) const {
    if (prefix.empty()) return {};
    std::wstring p = StringUtil::StripDiacritics(StringUtil::ToLower(prefix));
    if (p.empty()) return {};

    std::vector<std::wstring> result;

    // Exact hits first
    auto ait = asciiSingleIndex_.find(p);
    if (ait != asciiSingleIndex_.end()) {
        for (auto& k : ait->second) {
            auto sit = singleMap_.find(k);
            if (sit != singleMap_.end()) {
                for (auto& v : sit->second) {
                    bool found = false;
                    for (auto& r : result) if (r == v) { found = true; break; }
                    if (!found) { result.push_back(v); if ((int)result.size() >= limit) return result; }
                }
            }
        }
    }

    // Longer prefix hits
    std::vector<std::wstring> longerKeys;
    for (auto& [asciiKey, _] : asciiSingleIndex_) {
        if (asciiKey.size() > p.size() && asciiKey.substr(0, p.size()) == p) {
            longerKeys.push_back(asciiKey);
        }
    }
    std::sort(longerKeys.begin(), longerKeys.end(), [](const std::wstring& a, const std::wstring& b) {
        if (a.size() != b.size()) return a.size() < b.size();
        return a < b;
    });

    for (auto& asciiKey : longerKeys) {
        if ((int)result.size() >= limit) break;
        auto ait2 = asciiSingleIndex_.find(asciiKey);
        if (ait2 == asciiSingleIndex_.end()) continue;
        for (auto& orig : ait2->second) {
            auto sit = singleMap_.find(orig);
            if (sit == singleMap_.end()) continue;
            for (auto& v : sit->second) {
                bool found = false;
                for (auto& r : result) if (r == v) { found = true; break; }
                if (!found) { result.push_back(v); if ((int)result.size() >= limit) break; }
            }
            if ((int)result.size() >= limit) break;
        }
    }
    return result;
}

std::vector<std::wstring> NomDictionary::LookupPrefix(const std::wstring& query, int limit, bool strict) const {
    if (query.empty()) return {};
    std::wstring qAscii = StringUtil::StripDiacritics(StringUtil::ToLower(query));
    qAscii.erase(std::remove(qAscii.begin(), qAscii.end(), L' '), qAscii.end());
    if (qAscii.empty()) return {};

    std::vector<std::wstring> result;
    for (auto& [asciiKey, originals] : asciiWordIndex_) {
        if ((int)result.size() >= limit) break;
        if (asciiKey.size() > qAscii.size() && asciiKey.substr(0, qAscii.size()) == qAscii) {
            for (auto& orig : originals) {
                auto wit = wordMap_.find(orig);
                if (wit == wordMap_.end()) continue;
                for (auto& v : wit->second) {
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

std::vector<std::wstring> NomDictionary::Lookup(const std::wstring& query, bool strict) const {
    if (query.empty()) return {};
    std::vector<std::wstring> merged;

    auto words = LookupWord(query, strict);
    merged.insert(merged.end(), words.begin(), words.end());

    auto singles = LookupSingle(query, strict);
    for (auto& s : singles) {
        bool found = false;
        for (auto& m : merged) if (m == s) { found = true; break; }
        if (!found) merged.push_back(s);
    }

    auto prefixes = LookupPrefix(query, 24, strict);
    for (auto& p : prefixes) {
        bool found = false;
        for (auto& m : merged) if (m == p) { found = true; break; }
        if (!found) merged.push_back(p);
    }

    // Single prefix
    if (query.find(L' ') == std::wstring::npos) {
        auto sp = LookupSinglePrefix(query, 500, strict);
        for (auto& s : sp) {
            bool found = false;
            for (auto& m : merged) if (m == s) { found = true; break; }
            if (!found) merged.push_back(s);
        }
    }
    return merged;
}

bool NomDictionary::HasAsciiSinglePrefix(const std::wstring& asciiLower) const {
    if (asciiLower.empty()) return true;
    for (auto& [k, _] : asciiSingleIndex_) {
        if (k.size() >= asciiLower.size() && k.substr(0, asciiLower.size()) == asciiLower)
            return true;
    }
    return false;
}

bool NomDictionary::IsAsciiSingleKey(const std::wstring& asciiLower) const {
    return asciiSingleIndex_.count(asciiLower) > 0;
}

std::vector<std::pair<std::wstring, std::vector<std::wstring>>>
NomDictionary::LookupWordByVietTat(const std::vector<std::wstring>& segments, int limit) const {
    if (segments.size() < 2) return {};
    int n = (int)segments.size();
    std::wstring firstChar = segments[0].substr(0, 1);
    std::wstring bucketKey = std::to_wstring(n) + L"|" + firstChar;

    auto bit = wordsBySyllableCountAndFirstChar_.find(bucketKey);
    if (bit == wordsBySyllableCountAndFirstChar_.end()) return {};

    std::vector<std::pair<std::wstring, std::vector<std::wstring>>> result;
    for (auto& origKey : bit->second) {
        if ((int)result.size() >= limit) break;
        auto sit = wordSyllablesAscii_.find(origKey);
        if (sit == wordSyllablesAscii_.end()) continue;
        auto& asciiSylls = sit->second;
        if ((int)asciiSylls.size() != n) continue;

        bool ok = true;
        for (int i = 0; i < n; i++) {
            if (asciiSylls[i].size() < segments[i].size() ||
                asciiSylls[i].substr(0, segments[i].size()) != segments[i]) {
                ok = false; break;
            }
        }
        if (!ok) continue;

        auto wit = wordMap_.find(origKey);
        if (wit == wordMap_.end()) continue;
        result.push_back({ origKey, wit->second });
    }
    return result;
}

std::wstring NomDictionary::FindBundledKeyForNomByVietTat(const std::wstring& nom,
    const std::vector<std::wstring>& segments) const
{
    if (nom.empty() || segments.empty()) return L"";
    int n = (int)segments.size();
    std::wstring firstChar = segments[0].substr(0, 1);
    std::wstring bucketKey = std::to_wstring(n) + L"|" + firstChar;

    auto bit = wordsBySyllableCountAndFirstChar_.find(bucketKey);
    if (bit == wordsBySyllableCountAndFirstChar_.end()) return L"";

    for (auto& origKey : bit->second) {
        auto sit = wordSyllablesAscii_.find(origKey);
        if (sit == wordSyllablesAscii_.end()) continue;
        auto& asciiSylls = sit->second;
        if ((int)asciiSylls.size() != n) continue;

        bool ok = true;
        for (int i = 0; i < n; i++) {
            if (asciiSylls[i].size() < segments[i].size() ||
                asciiSylls[i].substr(0, segments[i].size()) != segments[i]) {
                ok = false; break;
            }
        }
        if (!ok) continue;

        auto wit = wordMap_.find(origKey);
        if (wit == wordMap_.end()) continue;
        for (auto& v : wit->second) {
            if (v == nom) return origKey;
        }
    }
    return L"";
}

std::wstring NomDictionary::LookupAsciiReadingForNom(const std::wstring& nom,
    const std::wstring& preferPrefix) const
{
    if (nom.empty()) return L"";
    auto it = nomToSingleReadings_.find(nom);
    if (it == nomToSingleReadings_.end()) return L"";
    auto& readings = it->second;
    if (readings.empty()) return L"";

    if (!preferPrefix.empty()) {
        std::wstring p = StringUtil::StripDiacritics(StringUtil::ToLower(preferPrefix));
        for (auto& r : readings) {
            if (StringUtil::StripDiacritics(r).substr(0, p.size()) == p) return r;
        }
    }
    return readings[0];
}
