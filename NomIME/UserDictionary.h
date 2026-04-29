// UserDictionary.h — User-defined dictionary
#pragma once
#include "Globals.h"

class UserDictionary {
public:
    static UserDictionary& Instance();

    void EnsureLoaded();
    void PutEntry(const std::wstring& key, const std::vector<std::wstring>& candidates);
    void RemoveEntry(const std::wstring& key);
    std::vector<std::wstring> LookupSingle(const std::wstring& query, bool strict = false) const;
    std::vector<std::wstring> LookupWord(const std::wstring& query, bool strict = false) const;
    std::vector<std::wstring> LookupPrefix(const std::wstring& query, int limit = 16) const;
    std::vector<std::pair<std::wstring, std::vector<std::wstring>>>
        LookupWordByVietTat(const std::vector<std::wstring>& segments, int limit = 24) const;
    int Size() const;

private:
    UserDictionary() = default;
    void Persist() const;
    void LoadFromFile();
    std::wstring GetFilePath() const;

    bool loaded_ = false;
    mutable std::mutex mutex_;
    // key -> list of Nom candidates
    std::unordered_map<std::wstring, std::vector<std::wstring>> entries_;
    // ascii key -> list of original keys
    std::unordered_map<std::wstring, std::vector<std::wstring>> asciiIndex_;
};
