#include "URLCodec.h"

#include <cctype>
#include <cstdio>

namespace {

    int HexVal(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    bool IsUnreserved(unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
    }

} // namespace

namespace URLCodec {

    std::string Decode(const std::string &input) {
        std::string out;
        out.reserve(input.size());

        for (size_t i = 0; i < input.size(); ++i) {
            char c = input[i];
            if (c == '%' && i + 2 < input.size()) {
                int hi = HexVal(input[i + 1]);
                int lo = HexVal(input[i + 2]);
                if (hi >= 0 && lo >= 0) {
                    out.push_back(static_cast<char>((hi << 4) | lo));
                    i += 2;
                    continue;
                }
            }
            out.push_back(c);
        }

        return out;
    }

    std::string Encode(const std::string &input) {
        static const char *hex = "0123456789ABCDEF";
        std::string out;
        out.reserve(input.size() * 3);

        for (unsigned char c : input) {
            if (IsUnreserved(c)) {
                out.push_back(static_cast<char>(c));
            } else {
                out.push_back('%');
                out.push_back(hex[(c >> 4) & 0xF]);
                out.push_back(hex[c & 0xF]);
            }
        }

        return out;
    }

} // namespace URLCodec
