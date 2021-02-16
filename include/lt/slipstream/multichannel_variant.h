#pragma once

#include <variant>

namespace lt::slipstream {

template <typename T>
using get_header_type = typename T::header_type;

template <typename T>
using get_data_type = typename T::data_type;

template <typename T>
using get_delta_type = typename T::delta_type;

template <typename... Ts>
class Variant {
   public:
    using header_type = typename std::variant<std::monostate, get_header_type<Ts>...>;
    using data_type = typename std::variant<std::monostate, get_data_type<Ts>...>;
    using delta_type = typename std::variant<std::monostate, get_delta_type<Ts>...>;
    using header_map = typename std::vector<std::pair<std::string, std::optional<header_type>>>;
};

} // namespace lt::slipstream
