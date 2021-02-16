#pragma once

#include <variant>

#include "seasocks/PageHandler.h"
#include "seasocks/PrintfLogger.h"
#include "seasocks/Server.h"
#include "seasocks/StringUtil.h"
#include "seasocks/SimpleResponse.h"

class UriMatcher {
   public:
    UriMatcher(std::vector<std::string> paths)
    {
        for (auto&& p : paths) {
            if (p.back() == '*') {
                p.pop_back();
                patterns_.push_back(Prefix{p});
            } else {
                patterns_.push_back(Exact{p});
            }
        }
    }

    bool matches(const std::string& uri) {
        for (auto&& pattern : patterns_) {
            if (match(pattern, uri)) {
                return true;
            }
        }

        return false;
    }

   private:
    struct Exact {
        std::string exact;
    };

    struct Prefix {
        std::string prefix;
    };

    using Pattern = std::variant<Exact, Prefix>;

    bool match(const Pattern& pattern, const std::string& s) {
        if (auto e = std::get_if<Exact>(&pattern)) {
            return s == e->exact;
        } else {
            auto p = std::get<Prefix>(pattern);
            return std::equal(
                p.prefix.begin(),
                p.prefix.begin() + std::min( p.prefix.size(), s.size() ),
                s.begin() );
        }
    }

    std::vector<Pattern> patterns_;
};

class DynamicPageHandler : public seasocks::PageHandler {
public:
    DynamicPageHandler(const std::vector<std::string>& static_paths)
        : static_matcher_(static_paths)
    {
    }

    std::shared_ptr<seasocks::Response> handle(const seasocks::Request& request) {
        using namespace seasocks;

        if (request.verb() != Request::Verb::Get) {
            return Response::unhandled();
        }

        auto uri = request.getRequestUri();
        if (static_matcher_.matches(uri)) {
            return Response::unhandled();
        }

        return handle_dynamic(request);
    }

    virtual std::shared_ptr<seasocks::Response> handle_dynamic(const seasocks::Request& request) = 0;

   private:
    UriMatcher static_matcher_;
};

