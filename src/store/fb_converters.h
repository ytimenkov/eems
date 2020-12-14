#ifndef EEMS_FB_CONVERTERS_H
#define EEMS_FB_CONVERTERS_H

#include "schema_generated.h"

#include <string>

namespace eems
{

template <typename TKey>
auto serialize_key(TKey const& key) -> std::string
{
    auto fbb = flatbuffers::FlatBufferBuilder{};
    auto key_buffer = CreateLibraryKey(fbb, KeyUnionTraits<TKey>::enum_value, fbb.CreateStruct(key).Union());
    fbb.Finish(key_buffer);
    return std::string{reinterpret_cast<char const*>(fbb.GetBufferPointer()), fbb.GetSize()};
}

template <typename Char>
inline auto put_string_view(std::basic_string_view<Char> data, flatbuffers::FlatBufferBuilder& fbb)
    -> flatbuffers::Offset<flatbuffers::Vector<uint8_t>>
{
    static_assert(sizeof(Char) == 1);
    uint8_t* buf{nullptr};
    auto result = fbb.CreateUninitializedVector(data.length() + 1, sizeof(Char), &buf);
    buf[data.copy(reinterpret_cast<Char*>(buf), data.size(), 0)] = 0;
    return result;
}

template <typename Char>
inline auto put_string(std::basic_string<Char> const& data, flatbuffers::FlatBufferBuilder& fbb)
    -> flatbuffers::Offset<flatbuffers::Vector<uint8_t>>
{
    static_assert(sizeof(Char) == 1);
    return fbb.CreateVector(reinterpret_cast<uint8_t const*>(data.c_str()), data.length() + 1);
}

template <typename Char>
inline auto as_string_view(flatbuffers::Vector<uint8_t> const& vec)
    -> std::basic_string_view<Char>
{
    static_assert(sizeof(Char) == 1);
    return {reinterpret_cast<Char const*>(vec.Data()), vec.size() - 1};
}

template <typename Char>
inline auto as_cstring(flatbuffers::Vector<uint8_t> const& vec)
    -> Char const*
{
    static_assert(sizeof(Char) == 1);
    return {reinterpret_cast<Char const*>(vec.Data())};
}

inline auto loggable_u8_view(std::u8string_view u8_view) -> std::string_view
{
    return std::string_view{reinterpret_cast<char const*>(u8_view.data()), u8_view.size()};
}

}

#endif
