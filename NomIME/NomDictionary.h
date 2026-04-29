// NomDictionary.h — Nom character dictionary (ported from Android)
#pragma once
#include "Globals.h"

/**
 * Singleton Nom-character dictionary for Windows.
 * Loads nom_dict_single.tsv and nom_dict_word.tsv from the installation directory.
 * Provides lookup, prefix search, and viết tắt (abbreviated input) support.
 */
class NomDictionary {
public:
    static NomDictionary& Instance();

    // Load dictionaries from the given directory (typically install dir)
    void EnsureLoaded(const std::wstring& dataDir);
    bool IsLoaded() const { return loaded_; }

    // Lookup single syllable candidates
    std::vector<std::wstring> LookupSingle(const std::wstring& query, bool strict = false) const;

    // Lookup compound word candidates
    std::vector<std::wstring> LookupWord(const std::wstring& query, bool strict = false) const;

    // Combined lookup (word + single + prefix)
    std::vector<std::wstring> Lookup(const std::wstring& query, bool strict = false) const;

    // Prefix search for single syllable
    std::vector<std::wstring> LookupSinglePrefix(const std::wstring& prefix, int limit = 500, bool strict = false) const;

    // Prefix search for compound words
    std::vector<std::wstring> LookupPrefix(const std::wstring& query, int limit = 24, bool strict = false) const;

    // Viết tắt (abbreviated input) lookup
    std::vector<std::pair<std::wstring, std::vector<std::wstring>>>
        LookupWordByVietTat(const std::vector<std::wstring>& segments, int limit = 24) const;

    // Check if asciiLower is a prefix of any single-syllable key
    bool HasAsciiSinglePrefix(const std::wstring& asciiLower) const;

    // Check if asciiLower is an exact single-syllable key
    bool IsAsciiSingleKey(const std::wstring& asciiLower) const;

    // Strip diacritics (delegated to StringUtil)
    static std::wstring StripDiacritics(const std::wstring& s);

    // Find bundled key for nom by viết tắt segments
    std::wstring FindBundledKeyForNomByVietTat(const std::wstring& nom,
        const std::vector<std::wstring>& segments) const;

    // Lookup ASCII reading for a Nom character
    std::wstring LookupAsciiReadingForNom(const std::wstring& nom,
        const std::wstring& preferPrefix = L"") const;

private:
    NomDictionary() = default;
    NomDictionary(const NomDictionary&) = delete;
    void operator=(const NomDictionary&) = delete;

    void LoadTsv(const std::wstring& path,
        std::unordered_map<std::wstring, std::vector<std::wstring>>& target,
        std::unordered_map<std::wstring, std::vector<std::wstring>>& asciiIdx);

    void BuildVietTatIndex();
    void BuildNomReverseIndex();

    bool loaded_ = false;
    mutable std::mutex mutex_;

    // Single syllable: key (Vietnamese with diacritics) -> list of Nom characters
    std::unordered_map<std::wstring, std::vector<std::wstring>> singleMap_;
    // Compound word: key -> list of Nom words
    std::unordered_map<std::wstring, std::vector<std::wstring>> wordMap_;
    // ASCII index: stripped key -> list of original keys
    std::unordered_map<std::wstring, std::vector<std::wstring>> asciiSingleIndex_;
    std::unordered_map<std::wstring, std::vector<std::wstring>> asciiWordIndex_;

    // Viết tắt indices
    std::unordered_map<std::wstring, std::vector<std::wstring>> wordSyllablesAscii_; // key -> ascii syllables
    std::unordered_map<int, std::vector<std::wstring>> wordsBySyllableCount_;
    std::unordered_map<std::wstring, std::vector<std::wstring>> wordsBySyllableCountAndFirstChar_;

    // Reverse index: Nom char -> list of readings (tone-preserving)
    std::unordered_map<std::wstring, std::vector<std::wstring>> nomToSingleReadings_;
};
