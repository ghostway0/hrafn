#pragma once

#define try_unwrap(x) \
    ({ \
        auto _x = x; \
        if (!_x.has_value()) { \
            return std::unexpected(_x.error()); \
        } \
        _x.value(); \
    })

#define try_unwrap_or(x, err) \
    ({ \
        auto _x = x; \
        if (!_x.has_value()) { \
            return std::unexpected(err); \
        } \
        _x.value(); \
    })

#define co_try_unwrap_or(x, err) \
    ({ \
        auto _x = x; \
        if (!_x.has_value()) { \
            co_return std::unexpected(err); \
        } \
        _x.value(); \
    })

#define try_unwrap_optional(x) \
    ({ \
        auto _x = x; \
        if (!_x.has_value()) { \
            return std::nullopt; \
        } \
        _x.value(); \
    })

#define try_unwrap_optional_or(x, err) \
    ({ \
        auto _x = x; \
        if (!_x.has_value()) { \
            return std::unexpected(err); \
        } \
        _x.value(); \
    })
