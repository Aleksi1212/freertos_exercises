#include <utils.h>

std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        tokens.push_back(item);
    }

    return tokens;
}

bool to_uint32(const std::string& str, uint32_t& out) {
    unsigned int temp = 0;  // use wider type for parsing

    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), temp);

    if (ec == std::errc() && ptr == str.data() + str.size() && temp <= UINT32_MAX) {
        out = static_cast<uint32_t>(temp);
        return true;
    }
    return false;
}