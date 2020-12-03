#ifndef EEMS_SPIRIT_H
#define EEMS_SPIRIT_H

#include <boost/spirit/home/x3.hpp>

namespace eems
{
namespace x3 = boost::spirit::x3;

template <typename Parser, typename Attribute, typename CharT>
inline auto parse(std::basic_string_view<CharT> text, Parser const& p, Attribute& attr)
{
    auto first = text.begin();
    auto const res = x3::parse(first, text.end(), p, attr);
    if (!res || first != text.end())
        return false;
    return res;
}

}

#endif