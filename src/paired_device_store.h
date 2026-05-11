#pragma once

#include <cstring>
#include <string>
#include <vector>

class PairedDeviceStore {
   public:
    static constexpr int MAX_PAIRED = 3;

    bool isPaired(const std::string& addr) const;
    void add(const std::string& addr);
    void clear();
    int count() const;

    std::string serialize() const;
    void deserialize(const std::string& data);

   private:
    std::vector<std::string> addresses_;
};
