// TelexEngine.cpp — Telex Vietnamese input engine (ported from Android Kotlin)
#include "TelexEngine.h"
#include "StringUtil.h"

// ========== Static data tables ==========

const std::vector<TelexEngine::ToneEntry>& TelexEngine::GetToneTable() {
    static std::vector<ToneEntry> table = {
        {L'a', {L'a',L'á',L'à',L'ả',L'ã',L'ạ'}},
        {L'ă', {L'ă',L'ắ',L'ằ',L'ẳ',L'ẵ',L'ặ'}},
        {L'â', {L'â',L'ấ',L'ầ',L'ẩ',L'ẫ',L'ậ'}},
        {L'e', {L'e',L'é',L'è',L'ẻ',L'ẽ',L'ẹ'}},
        {L'ê', {L'ê',L'ế',L'ề',L'ể',L'ễ',L'ệ'}},
        {L'i', {L'i',L'í',L'ì',L'ỉ',L'ĩ',L'ị'}},
        {L'o', {L'o',L'ó',L'ò',L'ỏ',L'õ',L'ọ'}},
        {L'ô', {L'ô',L'ố',L'ồ',L'ổ',L'ỗ',L'ộ'}},
        {L'ơ', {L'ơ',L'ớ',L'ờ',L'ở',L'ỡ',L'ợ'}},
        {L'u', {L'u',L'ú',L'ù',L'ủ',L'ũ',L'ụ'}},
        {L'ư', {L'ư',L'ứ',L'ừ',L'ử',L'ữ',L'ự'}},
        {L'y', {L'y',L'ý',L'ỳ',L'ỷ',L'ỹ',L'ỵ'}},
        {L'A', {L'A',L'Á',L'À',L'Ả',L'Ã',L'Ạ'}},
        {L'Ă', {L'Ă',L'Ắ',L'Ằ',L'Ẳ',L'Ẵ',L'Ặ'}},
        {L'Â', {L'Â',L'Ấ',L'Ầ',L'Ẩ',L'Ẫ',L'Ậ'}},
        {L'E', {L'E',L'É',L'È',L'Ẻ',L'Ẽ',L'Ẹ'}},
        {L'Ê', {L'Ê',L'Ế',L'Ề',L'Ể',L'Ễ',L'Ệ'}},
        {L'I', {L'I',L'Í',L'Ì',L'Ỉ',L'Ĩ',L'Ị'}},
        {L'O', {L'O',L'Ó',L'Ò',L'Ỏ',L'Õ',L'Ọ'}},
        {L'Ô', {L'Ô',L'Ố',L'Ồ',L'Ổ',L'Ỗ',L'Ộ'}},
        {L'Ơ', {L'Ơ',L'Ớ',L'Ờ',L'Ở',L'Ỡ',L'Ợ'}},
        {L'U', {L'U',L'Ú',L'Ù',L'Ủ',L'Ũ',L'Ụ'}},
        {L'Ư', {L'Ư',L'Ứ',L'Ừ',L'Ử',L'Ữ',L'Ự'}},
        {L'Y', {L'Y',L'Ý',L'Ỳ',L'Ỷ',L'Ỹ',L'Ỵ'}},
    };
    return table;
}

const std::unordered_map<wchar_t, std::pair<wchar_t, int>>& TelexEngine::GetToneReverse() {
    static std::unordered_map<wchar_t, std::pair<wchar_t, int>> rev;
    static bool built = false;
    if (!built) {
        for (auto& e : GetToneTable()) {
            for (int i = 0; i < 6; i++) {
                rev[e.tones[i]] = { e.base, i };
            }
        }
        built = true;
    }
    return rev;
}

bool TelexEngine::CharIsVowelLike(wchar_t ch) {
    const auto& rev = GetToneReverse();
    if (rev.count(ch)) return true;
    static const std::wstring baseVowels = L"aAăĂâÂeEêÊiIoOôÔơƠuUưƯyY";
    return baseVowels.find(ch) != std::wstring::npos;
}

std::pair<wchar_t, int> TelexEngine::GetBaseTone(wchar_t ch) {
    const auto& rev = GetToneReverse();
    auto it = rev.find(ch);
    if (it != rev.end()) return it->second;
    return { ch, 0 };
}

wchar_t TelexEngine::GetTonedChar(wchar_t base, int tone) {
    for (auto& e : GetToneTable()) {
        if (e.base == base && tone >= 0 && tone < 6) return e.tones[tone];
    }
    return base;
}

int TelexEngine::GetToneTrigger(wchar_t ch) {
    switch (towlower(ch)) {
        case L's': return 1;
        case L'f': return 2;
        case L'r': return 3;
        case L'x': return 4;
        case L'j': return 5;
        case L'z': return 0;
        default: return -1;
    }
}

wchar_t TelexEngine::GetVowelMod(wchar_t existing, wchar_t trigger) {
    // Vowel modifier pairs (existing, trigger) -> merged
    using P = std::pair<wchar_t, wchar_t>;
    static std::unordered_map<uint32_t, wchar_t> map;
    static bool built = false;
    if (!built) {
        auto key = [](wchar_t a, wchar_t b) -> uint32_t { return ((uint32_t)a << 16) | (uint32_t)b; };
        map[key(L'a',L'a')] = L'â'; map[key(L'a',L'w')] = L'ă';
        map[key(L'e',L'e')] = L'ê'; map[key(L'o',L'o')] = L'ô';
        map[key(L'o',L'w')] = L'ơ'; map[key(L'u',L'w')] = L'ư';
        map[key(L'd',L'd')] = L'đ';
        map[key(L'A',L'A')] = L'Â'; map[key(L'A',L'W')] = L'Ă';
        map[key(L'E',L'E')] = L'Ê'; map[key(L'O',L'O')] = L'Ô';
        map[key(L'O',L'W')] = L'Ơ'; map[key(L'U',L'W')] = L'Ư';
        map[key(L'D',L'D')] = L'Đ';
        // Mixed case
        map[key(L'a',L'W')] = L'ă'; map[key(L'A',L'w')] = L'Ă';
        map[key(L'e',L'E')] = L'ê'; map[key(L'E',L'e')] = L'Ê';
        map[key(L'o',L'W')] = L'ơ'; map[key(L'O',L'w')] = L'Ơ';
        map[key(L'u',L'W')] = L'ư'; map[key(L'U',L'w')] = L'Ư';
        map[key(L'd',L'D')] = L'đ'; map[key(L'D',L'd')] = L'Đ';
        map[key(L'a',L'A')] = L'â'; map[key(L'A',L'a')] = L'Â';
        map[key(L'o',L'O')] = L'ô'; map[key(L'O',L'o')] = L'Ô';
        built = true;
    }
    uint32_t k = ((uint32_t)existing << 16) | (uint32_t)trigger;
    auto it = map.find(k);
    return (it != map.end()) ? it->second : 0;
}

std::wstring TelexEngine::GetUndoMerge(wchar_t existing, wchar_t trigger) {
    using P = std::pair<wchar_t, wchar_t>;
    static std::unordered_map<uint32_t, std::wstring> map;
    static bool built = false;
    if (!built) {
        auto key = [](wchar_t a, wchar_t b) -> uint32_t { return ((uint32_t)a << 16) | (uint32_t)b; };
        map[key(L'â',L'a')] = L"aa"; map[key(L'ă',L'w')] = L"aw";
        map[key(L'ê',L'e')] = L"ee"; map[key(L'ô',L'o')] = L"oo";
        map[key(L'ơ',L'w')] = L"ow"; map[key(L'ư',L'w')] = L"uw";
        map[key(L'đ',L'd')] = L"dd";
        map[key(L'Â',L'A')] = L"AA"; map[key(L'Ă',L'W')] = L"AW";
        map[key(L'Ê',L'E')] = L"EE"; map[key(L'Ô',L'O')] = L"OO";
        map[key(L'Ơ',L'W')] = L"OW"; map[key(L'Ư',L'W')] = L"UW";
        map[key(L'Đ',L'D')] = L"DD";
        // Mixed case retriggers
        map[key(L'Â',L'a')] = L"Aa"; map[key(L'Ă',L'w')] = L"Aw";
        map[key(L'Ê',L'e')] = L"Ee"; map[key(L'Ô',L'o')] = L"Oo";
        map[key(L'Ơ',L'w')] = L"Ow"; map[key(L'Ư',L'w')] = L"Uw";
        map[key(L'Đ',L'd')] = L"Dd";
        built = true;
    }
    uint32_t k = ((uint32_t)existing << 16) | (uint32_t)trigger;
    auto it = map.find(k);
    return (it != map.end()) ? it->second : L"";
}

std::wstring TelexEngine::GetHornFollowUp(wchar_t last, wchar_t ch) {
    if (last == L'ư' && ch == L'o') return L"ươ";
    if (last == L'Ư' && ch == L'o') return L"Ươ";
    if (last == L'ư' && ch == L'O') return L"ưƠ";
    if (last == L'Ư' && ch == L'O') return L"ƯƠ";
    return L"";
}

int TelexEngine::FindExistingToneIndex(const std::wstring& s, int toneIdx) {
    const auto& rev = GetToneReverse();
    for (int i = 0; i < (int)s.size(); i++) {
        auto it = rev.find(s[i]);
        if (it != rev.end() && it->second.second == toneIdx) return i;
    }
    return -1;
}

int TelexEngine::GlideSkipOffset(const std::wstring& s) {
    if (s.size() < 3) return 0;
    wchar_t c0 = towlower(s[0]);
    wchar_t c1 = towlower(s[1]);
    bool hasFurtherVowel = false;
    for (size_t i = 2; i < s.size(); i++) {
        if (CharIsVowelLike(s[i])) { hasFurtherVowel = true; break; }
    }
    if (!hasFurtherVowel) return 0;
    if (c0 == L'g' && c1 == L'i') return 2;
    if (c0 == L'q' && c1 == L'u') return 2;
    return 0;
}

bool TelexEngine::IsLastSyllablePhonotacticallyValid(const std::wstring& composing) {
    int start = (int)composing.size();
    for (int i = (int)composing.size() - 1; i >= 0; i--) {
        if (!StringUtil::IsLetter(composing[i])) break;
        start = i;
    }
    if (start >= (int)composing.size()) return false;

    int vowelRuns = 0;
    bool inVowel = false;
    bool sawTonedVowel = false;
    int vowelsAfterTone = 0;
    const auto& rev = GetToneReverse();

    for (int i = start; i < (int)composing.size(); i++) {
        wchar_t ch = composing[i];
        bool v = CharIsVowelLike(ch);
        if (v && !inVowel) {
            vowelRuns++;
            if (vowelRuns >= 2) return false;
        }
        if (v && sawTonedVowel) vowelsAfterTone++;
        auto it = rev.find(ch);
        if (it != rev.end() && it->second.second != 0) sawTonedVowel = true;
        inVowel = v;
    }
    if (sawTonedVowel && vowelsAfterTone >= 2) return false;
    if (vowelRuns != 1) return false;

    // Reject naked double-same-vowel clusters (aa/ee/oo)
    std::wstring syllable = composing.substr(start);
    int vStart = -1;
    for (int i = 0; i < (int)syllable.size(); i++) {
        if (CharIsVowelLike(syllable[i])) { vStart = i; break; }
    }
    if (vStart < 0) return false;
    int vEnd = vStart;
    while (vEnd < (int)syllable.size() && CharIsVowelLike(syllable[vEnd])) vEnd++;
    std::wstring cluster;
    for (int i = vStart; i < vEnd; i++) cluster += towlower(syllable[i]);
    if (cluster == L"aa" || cluster == L"ee" || cluster == L"oo") return false;
    return true;
}

int TelexEngine::FindToneTargetIndex(const std::wstring& s, bool toneStyleOld) {
    int scanStart = GlideSkipOffset(s);

    // Rule 2: modified vowels take priority
    static const std::wstring priority = L"ơƠêÊâÂôÔăĂ";
    for (int i = (int)s.size() - 1; i >= scanStart; i--) {
        if (priority.find(s[i]) != std::wstring::npos) return i;
    }

    // Collect vowel positions from scanStart
    std::vector<int> vowelIdx;
    for (int i = scanStart; i < (int)s.size(); i++) {
        if (CharIsVowelLike(s[i])) vowelIdx.push_back(i);
    }
    if (vowelIdx.empty()) {
        for (int i = 0; i < (int)s.size(); i++) {
            if (CharIsVowelLike(s[i])) vowelIdx.push_back(i);
        }
    }
    if (vowelIdx.empty()) return -1;
    if (vowelIdx.size() == 1) return vowelIdx[0];

    int lastVowel = vowelIdx.back();
    bool hasTrailingConsonant = lastVowel < (int)s.size() - 1;
    if (hasTrailingConsonant) return lastVowel;

    int firstVowel = vowelIdx[vowelIdx.size() - 2];
    wchar_t firstCh = towlower(s[firstVowel]);
    wchar_t secondCh = towlower(s[lastVowel]);

    if (firstCh == secondCh) return firstVowel;
    if ((firstCh == L'u' || firstCh == L'ư') && secondCh == L'a') return firstVowel;

    bool isRisingDiphthong = (firstCh == L'o' || firstCh == L'u') && secondCh != L'i';
    if (!isRisingDiphthong) return firstVowel;
    return toneStyleOld ? firstVowel : lastVowel;
}

std::wstring TelexEngine::RelocateTone(const std::wstring& composing, wchar_t ch, bool toneStyleOld) {
    int syllableStart = (int)composing.size();
    for (int i = (int)composing.size() - 1; i >= 0; i--) {
        if (!StringUtil::IsLetter(composing[i])) break;
        syllableStart = i;
    }
    if (syllableStart >= (int)composing.size()) return L"";

    int oldToneIdx = -1;
    int oldToneNum = 0;
    const auto& rev = GetToneReverse();
    for (int i = syllableStart; i < (int)composing.size(); i++) {
        auto it = rev.find(composing[i]);
        if (it != rev.end() && it->second.second != 0) {
            oldToneIdx = i;
            oldToneNum = it->second.second;
            break;
        }
    }
    if (oldToneIdx < 0) return L"";

    wchar_t oldBase = GetBaseTone(composing[oldToneIdx]).first;
    std::wstring stripped = composing;
    stripped[oldToneIdx] = oldBase;
    stripped += ch;

    if (!IsLastSyllablePhonotacticallyValid(stripped)) return L"";

    int newIdx = FindToneTargetIndex(stripped, toneStyleOld);
    if (newIdx < 0 || newIdx == oldToneIdx) return L"";

    auto [targetBase, _] = GetBaseTone(stripped[newIdx]);
    wchar_t newCh = GetTonedChar(targetBase, oldToneNum);
    if (newCh == targetBase) return L"";
    stripped[newIdx] = newCh;
    return stripped;
}

std::pair<wchar_t, wchar_t> TelexEngine::LongDistanceHatFor(wchar_t ch) {
    switch (ch) {
        case L'a': return {L'a', L'â'};
        case L'e': return {L'e', L'ê'};
        case L'o': return {L'o', L'ô'};
        case L'A': return {L'A', L'Â'};
        case L'E': return {L'E', L'Ê'};
        case L'O': return {L'O', L'Ô'};
        default: return {0, 0};
    }
}

std::wstring TelexEngine::RewriteHatLongDistance(const std::wstring& composing, wchar_t targetBase, wchar_t hatChar) {
    for (int i = (int)composing.size() - 1; i >= 0; i--) {
        wchar_t c = composing[i];
        if (!StringUtil::IsLetter(c)) break;
        auto [baseOfC, toneOfC] = GetBaseTone(c);
        if (baseOfC == targetBase) {
            wchar_t repl = GetTonedChar(hatChar, toneOfC);
            std::wstring result = composing.substr(0, i) + repl + composing.substr(i + 1);
            return result;
        }
        // Stop if we hit a vowel that already has the hat base
        auto [hatBase, _] = GetBaseTone(hatChar);
        if (baseOfC == hatBase) return L"";
    }
    return L"";
}

std::wstring TelexEngine::ApplyClusterW(const std::wstring& composing, size_t syllableStart) {
    std::wstring syllableSub = composing.substr(syllableStart);
    int postGlideOffset = 0;
    if (syllableSub.size() >= 3) {
        wchar_t c0 = towlower(syllableSub[0]);
        wchar_t c1 = towlower(syllableSub[1]);
        bool hasFurther = false;
        for (size_t i = 2; i < syllableSub.size(); i++) {
            if (CharIsVowelLike(syllableSub[i])) { hasFurther = true; break; }
        }
        if (hasFurther && ((c0 == L'g' && c1 == L'i') || (c0 == L'q' && c1 == L'u')))
            postGlideOffset = 2;
    }
    size_t clusterSearchStart = syllableStart + postGlideOffset;

    int clusterStart = -1;
    size_t i = clusterSearchStart;
    while (i < composing.size()) {
        if (CharIsVowelLike(composing[i])) { clusterStart = (int)i; break; }
        i++;
    }
    if (clusterStart < 0) return L"";
    i = clusterStart;
    while (i < composing.size() && CharIsVowelLike(composing[i])) i++;
    size_t clusterEnd = i;

    while (i < composing.size()) {
        if (CharIsVowelLike(composing[i])) return L"";
        i++;
    }

    std::wstring baseChars;
    for (int k = clusterStart; k < (int)clusterEnd; k++) {
        auto [base, _] = GetBaseTone(composing[k]);
        wchar_t plain = towlower(base);
        if (plain == L'ă' || plain == L'â') plain = L'a';
        else if (plain == L'ê') plain = L'e';
        else if (plain == L'ô' || plain == L'ơ') plain = L'o';
        else if (plain == L'ư') plain = L'u';
        baseChars += plain;
    }

    // Action codes: 0=keep, 1=horn, 2=breve
    std::vector<int> actions;
    if (baseChars == L"a") actions = {2};
    else if (baseChars == L"o") actions = {1};
    else if (baseChars == L"u") actions = {1};
    else if (baseChars == L"oi") actions = {1, 0};
    else if (baseChars == L"ui") actions = {1, 0};
    else if (baseChars == L"ua") actions = {1, 0};
    else if (baseChars == L"uu") actions = {1, 0};
    else if (baseChars == L"oa") actions = {0, 2};
    else if (baseChars == L"uo") actions = {1, 1};
    else if (baseChars == L"uoi") actions = {1, 1, 0};
    else return L"";

    // Check existing modifications
    for (int k = clusterStart; k < (int)clusterEnd; k++) {
        auto [base, _] = GetBaseTone(composing[k]);
        if (base == L'â' || base == L'Â' || base == L'ê' || base == L'Ê' ||
            base == L'ô' || base == L'Ô') return L"";
    }

    std::wstring sb = composing;
    for (size_t idx = 0; idx < actions.size(); idx++) {
        if (actions[idx] == 0) continue;
        int pos = clusterStart + (int)idx;
        auto [entry_base, tone] = GetBaseTone(composing[pos]);
        wchar_t newBase;
        if (actions[idx] == 2) { // breve
            if (entry_base == L'a') newBase = L'ă';
            else if (entry_base == L'A') newBase = L'Ă';
            else return L"";
        } else { // horn
            if (entry_base == L'o') newBase = L'ơ';
            else if (entry_base == L'O') newBase = L'Ơ';
            else if (entry_base == L'u') newBase = L'ư';
            else if (entry_base == L'U') newBase = L'Ư';
            else return L"";
        }
        wchar_t repl = GetTonedChar(newBase, tone);
        sb[pos] = repl;
    }
    return sb;
}

std::wstring TelexEngine::RewriteHornForW(const std::wstring& composing, bool triggerUpper) {
    wchar_t literalW = triggerUpper ? L'W' : L'w';

    // Find syllable boundary
    int syllableStart = (int)composing.size();
    for (int i = (int)composing.size() - 1; i >= 0; i--) {
        if (!StringUtil::IsLetter(composing[i])) break;
        syllableStart = i;
    }

    std::wstring syllable = composing.substr(syllableStart);

    // Case 2: undo existing w-produced vowels
    bool hasWVowel = false;
    for (wchar_t c : syllable) {
        if (c == L'ă' || c == L'Ă' || c == L'ơ' || c == L'Ơ' || c == L'ư' || c == L'Ư') {
            hasWVowel = true; break;
        }
    }
    if (hasWVowel) {
        std::wstring sb;
        for (wchar_t c : syllable) {
            switch (c) {
                case L'ă': sb += L'a'; break;
                case L'Ă': sb += L'A'; break;
                case L'ơ': sb += L'o'; break;
                case L'Ơ': sb += L'O'; break;
                case L'ư': sb += L'u'; break;
                case L'Ư': sb += L'U'; break;
                default: sb += c; break;
            }
        }
        return composing.substr(0, syllableStart) + sb + literalW;
    }

    // Case 3: cluster-based rewrite
    std::wstring rewritten = ApplyClusterW(composing, syllableStart);
    if (!rewritten.empty()) return rewritten;

    // Fallback
    std::wstring rest = composing.substr(syllableStart);
    if (!rest.empty()) {
        bool hasLetter = false, hasVowel = false;
        for (wchar_t c : rest) {
            if (StringUtil::IsLetter(c)) hasLetter = true;
            if (CharIsVowelLike(c)) hasVowel = true;
        }
        if (hasLetter) {
            if (!hasVowel) {
                return composing + (triggerUpper ? L'Ư' : L'ư');
            }
            return composing + literalW;
        }
    }
    return L"";
}

// ========== Main Apply function ==========

std::wstring TelexEngine::Apply(const std::wstring& composing, wchar_t ch, bool toneStyleOld) {
    // Standalone w -> ư
    if ((ch == L'w' || ch == L'W') && (composing.empty() || !StringUtil::IsLetter(composing.back()))) {
        return composing + (ch == L'W' ? L'Ư' : L'ư');
    }
    if (composing.empty()) return std::wstring(1, ch);

    // 0. Undo merge
    std::wstring undone = GetUndoMerge(composing.back(), ch);
    if (!undone.empty()) {
        return composing.substr(0, composing.size() - 1) + undone;
    }

    // 0b. Tone undo
    wchar_t loweredCh = towlower(ch);
    if (loweredCh == L's' || loweredCh == L'f' || loweredCh == L'r' ||
        loweredCh == L'x' || loweredCh == L'j') {
        int triggerTone = GetToneTrigger(loweredCh);
        int tonedIdx = FindExistingToneIndex(composing, triggerTone);
        if (tonedIdx >= 0) {
            auto [base, _] = GetBaseTone(composing[tonedIdx]);
            wchar_t arr0 = GetTonedChar(base, 0);
            std::wstring sb = composing;
            sb[tonedIdx] = arr0;
            sb += ch;
            return sb;
        }
    }

    // 1a. W key
    if (ch == L'w' || ch == L'W') {
        std::wstring rewritten = RewriteHornForW(composing, ch == L'W');
        if (!rewritten.empty()) return rewritten;
        return composing + (ch == L'W' ? L'Ư' : L'ư');
    }

    // 1b. Horn follow-up
    std::wstring horn = GetHornFollowUp(composing.back(), ch);
    if (!horn.empty()) {
        return composing.substr(0, composing.size() - 1) + horn;
    }

    // 1c. Vowel modifier
    wchar_t merged = GetVowelMod(composing.back(), ch);
    if (merged) {
        return composing.substr(0, composing.size() - 1) + merged;
    }

    // 1d. Long-distance hat
    auto [hatTarget, hatChar] = LongDistanceHatFor(ch);
    if (hatTarget) {
        std::wstring rewritten = RewriteHatLongDistance(composing, hatTarget, hatChar);
        if (!rewritten.empty()) return rewritten;
    }

    // 2. Tone marks
    int tone = GetToneTrigger(ch);
    bool hasVowel = false;
    for (wchar_t c : composing) if (CharIsVowelLike(c)) { hasVowel = true; break; }
    if (tone >= 0 && hasVowel && IsLastSyllablePhonotacticallyValid(composing)) {
        int idx = FindToneTargetIndex(composing, toneStyleOld);
        if (idx >= 0) {
            auto [base, _] = GetBaseTone(composing[idx]);
            wchar_t newCh = GetTonedChar(base, tone);
            std::wstring sb = composing;
            sb[idx] = newCh;
            return sb;
        }
    }

    // 2b. Tone relocation
    if (StringUtil::IsLetter(ch)) {
        std::wstring relocated = RelocateTone(composing, ch, toneStyleOld);
        if (!relocated.empty()) return relocated;
    }

    // 3. Default: append
    return composing + ch;
}

bool TelexEngine::IsTelexTriggerChar(wchar_t ch) {
    wchar_t lower = towlower(ch);
    return lower == L's' || lower == L'f' || lower == L'r' || lower == L'x' ||
           lower == L'j' || lower == L'z' || lower == L'a' || lower == L'w' ||
           lower == L'e' || lower == L'd' || lower == L'o';
}
