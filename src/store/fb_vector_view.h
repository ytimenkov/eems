#ifndef EEMS_FB_VECTOR_VIEW_H
#define EEMS_FB_VECTOR_VIEW_H

#include "../ranges.h"

#include <range/v3/view/facade.hpp>

namespace eems
{
template <typename Vector>
class fb_vector_view : public ranges::view_facade<fb_vector_view<Vector>>
{
public:
    fb_vector_view() = default;
    explicit fb_vector_view(Vector* data)
        : data_{data}
    {
    }

private:
    friend ranges::range_access;

    using reference_type = std::remove_pointer_t<typename Vector::mutable_return_type>;

    using return_type = std::conditional_t<std::is_const_v<Vector>, const reference_type, reference_type>;

    class cursor
    {
    public:
        cursor() noexcept = default;
        explicit cursor(typename Vector::const_iterator it) noexcept
            : it_{it} {}

        auto read() const -> return_type& { return const_cast<return_type&>(**it_); }
        auto next() noexcept -> void { ++it_; }
        auto equal(const cursor& rhs) const -> bool { return it_ == rhs.it_; }

    private:
        typename Vector::const_iterator it_;
    };

    auto begin_cursor() const { return data_ ? cursor{data_->cbegin()} : cursor{}; }
    auto end_cursor() const { return data_ ? cursor{data_->cend()} : cursor{}; }

private:
    Vector* data_{nullptr};
};
}

#endif
