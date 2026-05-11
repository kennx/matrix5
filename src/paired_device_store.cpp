#include "paired_device_store.h"

bool PairedDeviceStore::isPaired(const std::string& addr) const {
    for (const auto& a : addresses_) {
        if (a == addr) return true;
    }
    return false;
}

void PairedDeviceStore::add(const std::string& addr) {
    if (isPaired(addr)) return;
    if (static_cast<int>(addresses_.size()) >= MAX_PAIRED) {
        addresses_.erase(addresses_.begin());
    }
    addresses_.push_back(addr);
}

void PairedDeviceStore::clear() {
    addresses_.clear();
}

int PairedDeviceStore::count() const {
    return static_cast<int>(addresses_.size());
}

std::string PairedDeviceStore::serialize() const {
    std::string result;
    for (size_t i = 0; i < addresses_.size(); ++i) {
        if (i > 0) result += '\n';
        result += addresses_[i];
    }
    return result;
}

void PairedDeviceStore::deserialize(const std::string& data) {
    addresses_.clear();
    if (data.empty()) return;

    size_t start = 0;
    while (start < data.size()) {
        size_t end = data.find('\n', start);
        if (end == std::string::npos) {
            addresses_.push_back(data.substr(start));
            break;
        }
        addresses_.push_back(data.substr(start, end - start));
        start = end + 1;
    }
}
