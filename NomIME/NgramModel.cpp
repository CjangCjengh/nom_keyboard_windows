// NgramModel.cpp — N-gram frequency model implementation
#include "NgramModel.h"
#include "StringUtil.h"

static const wchar_t WSEP = L'\x01';

NgramModel& NgramModel::Instance() {
    static NgramModel instance;
    return instance;
}

std::wstring NgramModel::GetFilePath() const {
    return StringUtil::GetUserDataDir() + L"\\ngram.tsv";
}

void NgramModel::EnsureLoaded() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (loaded_) return;
    LoadFromFile();
    loaded_ = true;
}

void NgramModel::SetMaxN(int n) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxN_ = (std::max)(1, (std::min)(n, 6));
    while ((int)context_.size() > maxN_ - 1 && !context_.empty())
        context_.erase(context_.begin());
}

void NgramModel::ResetContext() {
    std::lock_guard<std::mutex> lock(mutex_);
    context_.clear();
}

void NgramModel::Observe(const std::wstring& token) {
    if (token.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);

    // Build all n-grams ending at token
    std::vector<std::wstring> buf = context_;
    buf.push_back(token);
    int start = (std::max)(0, (int)buf.size() - maxN_);
    for (int s = start; s < (int)buf.size(); s++) {
        std::wstring key;
        for (int i = s; i < (int)buf.size(); i++) {
            if (i > s) key += WSEP;
            key += buf[i];
        }
        counts_[key]++;
    }

    // Slide context
    context_.push_back(token);
    while ((int)context_.size() > maxN_ - 1 && !context_.empty())
        context_.erase(context_.begin());

    dirty_ = true;
    if (counts_.size() > MAX_ENTRIES) PruneLocked();

    // Debounced persist
    observeSinceFlush_++;
    if (observeSinceFlush_ >= 20) {
        observeSinceFlush_ = 0;
        // Persist without lock (we already hold it, so do inline)
        // Actually we need to release the lock first or do it async.
        // For simplicity, just mark dirty and persist on Deactivate.
    }
}

int NgramModel::Score(const std::wstring& candidate) const {
    if (candidate.empty()) return 0;
    std::lock_guard<std::mutex> lock(mutex_);
    int total = 0;
    std::vector<std::wstring> buf = context_;
    buf.push_back(candidate);
    int start = (std::max)(0, (int)buf.size() - maxN_);
    for (int s = start; s < (int)buf.size(); s++) {
        std::wstring key;
        for (int i = s; i < (int)buf.size(); i++) {
            if (i > s) key += WSEP;
            key += buf[i];
        }
        auto it = counts_.find(key);
        if (it != counts_.end()) {
            int len = (int)buf.size() - s;
            total += it->second * len;
        }
    }
    return total;
}

void NgramModel::PersistNow() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!dirty_) return;
    dirty_ = false;

    std::wstring path = GetFilePath();
    std::string utf8;
    for (auto& [k, v] : counts_) {
        // Replace WSEP with tab
        std::wstring line = k;
        for (auto& ch : line) { if (ch == WSEP) ch = L'\t'; }
        utf8 += StringUtil::WideToUTF8(line);
        utf8 += "\t";
        utf8 += std::to_string(v);
        utf8 += "\n";
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (file.is_open()) {
        file.write(utf8.c_str(), utf8.size());
    }
}

int NgramModel::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return (int)counts_.size();
}

void NgramModel::LoadFromFile() {
    std::wstring path = GetFilePath();
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    std::wstring wContent = StringUtil::UTF8ToWide(content);
    std::wistringstream stream(wContent);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty() || line[0] == L'#') continue;

        auto parts = StringUtil::Split(line, L'\t');
        if (parts.size() < 2) continue;

        // Last part is count, everything before is tokens
        int cnt = 0;
        try { cnt = std::stoi(parts.back()); } catch (...) { continue; }
        if (cnt <= 0) continue;

        // Rebuild key with WSEP
        std::wstring key;
        for (size_t i = 0; i + 1 < parts.size(); i++) {
            if (i > 0) key += WSEP;
            if (parts[i].empty()) { key.clear(); break; }
            key += parts[i];
        }
        if (!key.empty()) counts_[key] = cnt;
    }
}

void NgramModel::PruneLocked() {
    // Keep top half by count
    std::vector<std::pair<std::wstring, int>> entries(counts_.begin(), counts_.end());
    std::sort(entries.begin(), entries.end(), [](auto& a, auto& b) { return a.second > b.second; });
    counts_.clear();
    int keep = MAX_ENTRIES / 2;
    for (int i = 0; i < (std::min)(keep, (int)entries.size()); i++) {
        counts_[entries[i].first] = entries[i].second;
    }
}
