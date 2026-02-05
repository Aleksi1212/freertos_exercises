#ifndef UTILS_H
#define UTILS_H

#include <algorithm>
#include <vector>
#include <sstream>
#include <cstdint>
#include <charconv>

std::vector<std::string> split(const std::string& s, char delimiter);
bool to_uint32(const std::string& str, uint32_t& out);

#endif