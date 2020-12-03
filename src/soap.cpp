#include "soap.h"

#include "ranges.h"
#include "spirit.h"

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/sequence.hpp>
#include <range/v3/algorithm/starts_with.hpp>
#include <spdlog/spdlog.h>

BOOST_FUSION_ADAPT_STRUCT(
    eems::soap_action_info,
    service_id, action)

namespace eems
{
auto local_name(pugi::xml_node const& node)
{
    auto fqname = std::string_view{node.name()};
    if (auto const colon = fqname.find(':'); colon != fqname.npos)
    {
        fqname.remove_prefix(colon + 1);
    }
    return fqname;
}

auto parse_soap_request(http_request& req) -> soap_action_info
{
    using namespace x3;

    if (req.method() != http::verb::post)
    {
        throw http_error{http::status::bad_request, "Unsupported HTTP-method"};
    }

    // TODO: Optional parmeters may need to be parsed, like encoding.
    if (!req[http::field::content_type].starts_with("text/xml"))
    {
        throw http_error{http::status::unsupported_media_type, "Must be an XML"};
    }

    auto soap_info = soap_action_info{};
    if (!parse(req["soapaction"],
               ('"' >> +(char_ - '#') >> '#' >> +(char_ - '"') >> '"'),
               soap_info))
    {
        throw http_error{http::status::bad_request, "Invalid SOAPACTION header"};
    }

    auto buffer = req.body().data();

    if (auto parse_result = soap_info.doc.load_buffer_inplace(buffer.data(), buffer.size()); !parse_result)
    {
        throw http_error{http::status::bad_request, parse_result.description()};
    }

    soap_info.params = soap_info.doc.document_element().first_child().first_child();
    if (local_name(soap_info.params) != soap_info.action)
    {
        throw http_error{http::status::bad_request, "Invalid action element"};
    }
    return soap_info;
}
}
