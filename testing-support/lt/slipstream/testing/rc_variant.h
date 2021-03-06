#include <rapidcheck.h>

#include <variant>

namespace rc {

template<>
struct Arbitrary<std::monostate> {
    static Gen<std::monostate> arbitrary() {
        return gen::element(std::monostate());
    }
};

template <typename T, typename... Ts>
struct Arbitrary<std::variant<T, Ts...>> {
    static Gen<std::variant<T, Ts...>> arbitrary() {
        return gen::oneOf(
            gen::cast<std::variant<T, Ts...>>(gen::arbitrary<T>()),
            gen::cast<std::variant<T, Ts...>>(gen::arbitrary<Ts>())...);
    }
};

} // namespace rc

