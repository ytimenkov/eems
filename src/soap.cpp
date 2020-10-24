#include "soap.h"

#include <boost/fusion/sequence.hpp>
#include <boost/spirit/home/x3.hpp>
#include <range/v3/algorithm/starts_with.hpp>
#include <spdlog/spdlog.h>

namespace ranges = ::ranges;

namespace eems
{
template <typename Parser, typename Attribute>
auto parse(std::string_view text, Parser const& p, Attribute& attr)
{
    auto first = text.begin();
    auto const res = ::boost::spirit::x3::parse(first, text.end(), p, attr);
    if (!res || first != text.end())
        return false;
    return res;
}

auto handle_soap_request(http_request const& req) -> void
{
    using namespace boost::spirit::x3;
    using namespace std::literals;

    if (req.method() != http::verb::post)
    {
        throw http_error{http::status::bad_request, "Unsupported HTTP-method"};
    }

    // TODO: Optional parmeters may need to be parsed, like encoding.
    if (!ranges::starts_with(req[http::field::content_type], "text/xml"sv))
    {
        throw http_error{http::status::unsupported_media_type, "Must be an XML"};
    }

    auto soap_info = boost::fusion::vector<std::string, std::string>{};
    if (!parse(req["SOAPACTION"],
               ('"' >> +(char_ - '#') >> +(char_ - '"') >> '"'),
               soap_info))
    {
        throw http_error{http::status::bad_request, "Invalid SOAPACTION header"};
    }
}
}
