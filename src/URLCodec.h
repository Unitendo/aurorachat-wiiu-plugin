#pragma once

#include <string>

namespace URLCodec {

    std::string Decode(const std::string &input);

    std::string Encode(const std::string &input);

} // namespace URLCodec
