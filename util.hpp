//
// Created by Alex Gallon on 14/07/2020.
//

#ifndef GTASM_UTIL_HPP
#define GTASM_UTIL_HPP

#include <algorithm>
#include <cctype>
#include <locale>

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

// trim from start (copying)
static inline std::string ltrim_copy(std::string s) {
    ltrim(s);
    return s;
}

// trim from end (copying)
static inline std::string rtrim_copy(std::string s) {
    rtrim(s);
    return s;
}

// trim from both ends (copying)
static inline std::string trim_copy(std::string s) {
    trim(s);
    return s;
}

// I use this a lot, so:
using string_ref = const std::string &;

static std::vector<char> readFileBytes(char const* filename) {
    std::ifstream ifs(filename, std::ios::binary|std::ios::ate);
    std::ifstream::pos_type pos = ifs.tellg();

    std::vector<char>  result(pos);

    ifs.seekg(0, std::ios::beg);
    ifs.read(&result[0], pos);

    return result;
}

static void replaceAll(std::string &s, string_ref search, string_ref replace) {
    for (size_t pos = 0;; pos += replace.length()) {
        pos = s.find(search, pos);
        if (pos == std::string::npos) break;

        s.erase(pos, search.length());
        s.insert(pos, replace);
    }
}

template <class T>
inline int countDigits(T number) {
    int digits = 0;
    if (number < 0) digits = 1; // remove this line if '-' counts as a digit
    while (number) {
        number /= 10;
        digits++;
    }
    return digits;
}

static std::string replaceTokens(std::string s, const std::vector<std::string> &r) {
    for(int i = 0; i < r.size(); ++i) {
        std::string tokenStr = std::string("$") + std::to_string(i);
        replaceAll(s, tokenStr, r[i]);
    }

    return s;
}

static std::string cleanString(uint8_t *dirtyChars) {
    std::stringstream stream;

    int c;
    while(isascii(c = *(dirtyChars++))) {
        stream << (char)c;
    }

    return stream.str();
}

#include <chrono>
#include <ctime>

std::string currentDateString(){
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::string s(40, '\0');
    std::strftime(&s[0], s.size(), "%A %d %B %Y at %r", std::localtime(&now));
    return s;
}

std::string lastPathComponent(string_ref fullPath) {
    auto pos = fullPath.find_last_of("/\\");
    if(pos == std::string::npos) pos = 0;

    return fullPath.substr(pos + 1);
}

#include <algorithm>
#include <cctype>
#include <string>

template <typename S>
std::string stringUpper(S s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
    return s;
}

template <typename S>
std::string stringLower(S s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

template <typename T>
inline std::string to_string_hex(T v) {
    std::stringstream s;
    s << std::hex << v;

    return s.str();
}

#endif //GTASM_UTIL_HPP
