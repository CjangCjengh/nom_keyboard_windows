// NgramModel.h — N-gram frequency model for context-aware candidate re-ranking
#pragma once
#include "Globals.h"

/**
 * N-gram frequency model for Windows Nom IME.
 * Port of Android NgramModel.kt.
 *
 * Records ordered sequences of 1..N Nom tokens committed by the user.
 * Provides a score combining unigram frequency and conditional frequency
 * given recent context, used to re-rank candidates.
 *
 * Persistence: entries written to ngram.tsv in user data dir.
 */
class NgramModel {
public:
    static NgramModel& Instance();

    void EnsureLoaded();
    void SetMaxN(int n);
    int GetMaxN() const { return maxN_; }

    // Reset the running context (e.g. on sentence end, new document)
    void ResetContext();

    // Record that token was committed following current context
    void Observe(const std::wstring& token);

    // Return a score for candidate given current context (higher = more likely)
    int Score(const std::wstring& candidate) const;

    // Persist to disk immediately
    void PersistNow();

    // Size of model
    int Size() const;

private:
    NgramModel() = default;
    NgramModel(const NgramModel&) = delete;
    void operator=(const NgramModel&) = delete;

    void LoadFromFile();
    void PruneLocked();
    std::wstring GetFilePath() const;

    bool loaded_ = false;
    bool dirty_ = false;
    int maxN_ = 3;
    int observeSinceFlush_ = 0;
    mutable std::mutex mutex_;

    static const int MAX_ENTRIES = 20000;
    static const char SEP = '\x01';

    // key = "tok1\x01tok2\x01..." -> count
    std::unordered_map<std::wstring, int> counts_;
    // Ring buffer of most recently committed tokens
    std::vector<std::wstring> context_;
};
