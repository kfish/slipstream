#pragma once

#include <algorithm>
#include <cctype>
#include <boost/regex.hpp>

std::string submatch(const boost::cmatch& m, int idx) {
    const auto& sub = m[idx];
    return std::string(sub.first, sub.second);
}

// /path?query#fragment
class LocalUri {
   public:
    LocalUri(const std::string& str) {
        static const boost::regex uriRegex(
            "([^?#]*)"                    // path
            "(?:\\?([^#]*))?"             // ?query
            "(?:#(.*))?");                // #fragment

        boost::cmatch match;
        if (!boost::regex_match(str.c_str(), match, uriRegex)) {
            throw std::invalid_argument("invalid URI " + str);
        }

        path_ = submatch(match, 1);
        query_ = submatch(match, 2);
        fragment_ = submatch(match, 3);
    }

    const std::string& path() const { return path_; }
    const std::string& query() const { return query_; }
    const std::string& fragment() const { return fragment_; }

    /**
    * Get query parameters as key-value pairs.
    * e.g. for URI containing query string:  key1=foo&key2=&key3&=bar&=bar=
    * In returned list, there are 3 entries:
    *     "key1" => "foo"
    *     "key2" => ""
    *     "key3" => ""
    * Parts "=bar" and "=bar=" are ignored, as they are not valid query
    * parameters. "=bar" is missing parameter name, while "=bar=" has more than
    * one equal signs, we don't know which one is the delimiter for key and
    * value.
    *
    * Note, this method is not thread safe, it might update internal state, but
    * only the first call to this method update the state. After the first call
    * is finished, subsequent calls to this method are thread safe.
    *
    * @return  query parameter key-value pairs in a vector, each element is a
    *          pair of which the first element is parameter name and the second
    *          one is parameter value
    */
    const std::vector<std::pair<std::string, std::string>>& getQueryParams() {
        if (!query_.empty() && queryParams_.empty()) {
            // Parse query string
            static const boost::regex queryParamRegex(
                "(^|&)" /*start of query or start of parameter "&"*/
                "([^=&]*)=?" /*parameter name and "=" if value is expected*/
                "([^=&]*)" /*parameter value*/
                "(?=(&|$))" /*forward reference, next should be end of query or
                            start of next parameter*/);
            const boost::cregex_iterator paramBeginItr(
                query_.data(), query_.data() + query_.size(), queryParamRegex);
            boost::cregex_iterator paramEndItr;
            for (auto itr = paramBeginItr; itr != paramEndItr; ++itr) {
            if (itr->length(2) == 0) {
                // key is empty, ignore it
                continue;
            }
            queryParams_.emplace_back(
                std::string((*itr)[2].first, (*itr)[2].second), // parameter name
                std::string((*itr)[3].first, (*itr)[3].second) // parameter value
                );
            }
        }
        return queryParams_;
    }

   private:
    std::string path_;
    std::string query_;
    std::string fragment_;
    std::vector<std::pair<std::string, std::string>> queryParams_;
};
