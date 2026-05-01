// TelexEngine.h — Telex Vietnamese input engine (ported from Android Kotlin)
#pragma once
#include "Globals.h"

/**
 * Telex Vietnamese input engine for Windows.
 * Port of the Android TelexEngine.kt with identical behavior.
 *
 * Telex rules:
 *   Vowel modifiers: aa -> â,  aw -> ă,  ee -> ê,  oo -> ô,  ow/[ -> ơ,  uw/] -> ư,  dd -> đ
 *   Tone marks:      s -> sắc (acute),  f -> huyền (grave),  r -> hỏi (hook above),
 *                    x -> ngã (tilde),   j -> nặng (dot below),  z -> clear tone
 */
class TelexEngine {
public:
    // Core entry point: given the current composing buffer and a new character,
    // return the new composing buffer after applying Telex rules.
    //
    // lastWasStandaloneW tells the engine that the previous keystroke was a standalone-w
    // shortcut (i.e. the ư/Ư at the tail of composing was produced by the top branch,
    // NOT by a u+w merge or cluster rewrite). This distinguishes `w` + `w` (undo the
    // shortcut → literal `w`) from `u` + `w` + `w` (undo the merge → `uw`). Callers
    // should set this flag whenever their previous call hit the standalone branch and
    // clear it on any other buffer mutation (backspace, commit, …).
    static std::wstring Apply(const std::wstring& composing, wchar_t ch, bool toneStyleOld = true, bool lastWasStandaloneW = false);

    // Returns whether ch would potentially trigger a Telex transformation.
    static bool IsTelexTriggerChar(wchar_t ch);

    // Returns true iff passing (tail, ch) to Apply would land in one of the
    // "standalone-w" branches that append a brand-new ư/Ư (the top branch or
    // the pure-consonant-onset branch of rewriteHornForW). Callers use this to
    // set the lastWasStandaloneW flag for the NEXT call so that a subsequent
    // `w` keypress can be recognised as the "cancel the shortcut" second press.
    static bool WouldBeStandaloneW(const std::wstring& tail, wchar_t ch);

private:
    // Tone map: base vowel -> 6-element array [untoned, sắc, huyền, hỏi, ngã, nặng]
    struct ToneEntry {
        wchar_t base;
        wchar_t tones[6]; // [0]=no tone, [1]=acute, [2]=grave, [3]=hook, [4]=tilde, [5]=dot
    };

    static const std::vector<ToneEntry>& GetToneTable();
    static const std::unordered_map<wchar_t, std::pair<wchar_t, int>>& GetToneReverse();
    static int GetToneTrigger(wchar_t ch);
    static wchar_t GetVowelMod(wchar_t existing, wchar_t trigger);
    static std::wstring GetUndoMerge(wchar_t existing, wchar_t trigger);
    static std::wstring GetHornFollowUp(wchar_t last, wchar_t ch);

    static int FindToneTargetIndex(const std::wstring& s, bool toneStyleOld);
    static int FindExistingToneIndex(const std::wstring& s, int toneIdx);
    static int GlideSkipOffset(const std::wstring& s);
    static bool IsLastSyllablePhonotacticallyValid(const std::wstring& composing);
    static std::wstring RelocateTone(const std::wstring& composing, wchar_t ch, bool toneStyleOld);
    static std::wstring RewriteHornForW(const std::wstring& composing, bool triggerUpper);
    static std::wstring ApplyClusterW(const std::wstring& composing, size_t syllableStart);
    static std::pair<wchar_t, wchar_t> LongDistanceHatFor(wchar_t ch);
    static std::wstring RewriteHatLongDistance(const std::wstring& composing, wchar_t targetBase, wchar_t hatChar);

    static bool CharIsVowelLike(wchar_t ch);
    static wchar_t GetTonedChar(wchar_t base, int tone);
    static std::pair<wchar_t, int> GetBaseTone(wchar_t ch);
};
