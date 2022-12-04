#ifndef EEMS_AS_RESULT_H
#define EEMS_AS_RESULT_H

#include <boost/outcome/boost_result.hpp>
#include <boost/outcome/experimental/status_result.hpp>

namespace eems
{

// template <typename T, typename E = ::boost::outcome_v2::experimental::system_code>
// using result = ::boost::outcome_v2::experimental::status_result<T, E>;
template <typename T, typename EC>
using result = ::boost::outcome_v2::boost_result<T, EC>;
using ::boost::outcome_v2::success;

template <typename CompletionToken>
class as_result_t
{
public:
    /// Tag type used to prevent the "default" constructor from being used for
    /// conversions.
    struct default_constructor_tag
    {
    };

    constexpr as_result_t(
        default_constructor_tag = default_constructor_tag(),
        CompletionToken token = CompletionToken{})
        : token_(std::move(token))
    {
    }

    template <typename T>
    constexpr explicit as_result_t(T&& completion_token)
        : token_(std::forward<T>(completion_token))
    {
    }

    template <typename InnerExecutor>
    struct executor_with_default : InnerExecutor
    {
        typedef as_result_t default_completion_token_type;

        /// Construct the adapted executor from the inner executor type.
        executor_with_default(const InnerExecutor& ex) noexcept
            : InnerExecutor(ex)
        {
        }

        template <typename OtherExecutor>
        executor_with_default(const OtherExecutor& ex,
                              std::enable_if_t<std::is_convertible_v<OtherExecutor, InnerExecutor>>* = 0) noexcept
            : InnerExecutor(ex)
        {
        }
    };

    template <typename T>
    using as_default_on_t = typename T::template rebind_executor<
        executor_with_default<typename T::executor_type>>::other;

    template <typename T>
    static auto as_default_on(T&& object)
    {
        return typename std::decay_t<T>::template rebind_executor<
            executor_with_default<typename std::decay_t<T>::executor_type>>::other(std::forward<T>(object));
    }

    // private:
    CompletionToken token_;
};

template <typename CompletionToken>
inline constexpr auto as_result(CompletionToken&& completion_token)
{
    return as_result_t<typename std::decay_t<CompletionToken>>(
        std::forward<CompletionToken>(completion_token));
}

constexpr as_result_t<::boost::asio::use_awaitable_t<>> use_async_result{};

namespace detail
{

// Class to adapt a as_result_t as a completion handler.
template <typename Handler>
class as_result_handler
{
public:
    using result_type = void;

    template <typename CompletionToken>
    as_result_handler(as_result_t<CompletionToken> e)
        : handler_(std::move(e.token_))
    {
    }

    template <typename RedirectedHandler>
    as_result_handler(RedirectedHandler&& h)
        : handler_(std::forward<RedirectedHandler>(h))
    {
    }

    void operator()(::boost::system::error_code const& ec)
    {
        if (ec)
            handler_(result<void, ::boost::system::error_code>{ec});
        else
            handler_(result<void, ::boost::system::error_code>{success()});
    }

    template <typename Arg>
    void operator()(::boost::system::error_code const& ec, Arg&& arg)
    {
        using result_t = result<std::decay_t<Arg>, ::boost::system::error_code>;
        if (ec)
            handler_(result_t{ec});
        else
            handler_(result_t{std::forward<Arg>(arg)});
    }

    // private:
    Handler handler_;
};

template <typename Handler>
inline bool asio_handler_is_continuation(as_result_handler<Handler>* this_handler)
{
    return boost_asio_handler_cont_helpers::is_continuation(this_handler->handler_);
}

template <typename Signature>
struct as_result_signature
{
    typedef Signature type;
};

template <>
struct as_result_signature<void(::boost::system::error_code)>
{
    using type = void(result<void, ::boost::system::error_code>);
};

template <>
struct as_result_signature<void(::boost::system::error_code const&)>
    : as_result_signature<void(::boost::system::error_code)>
{
};

template <typename T>
struct as_result_signature<void(::boost::system::error_code, T)>
{
    using type = void(result<T, ::boost::system::error_code>);
};

template <typename T>
struct as_result_signature<void(::boost::system::error_code const&, T)>
    : as_result_signature<void(::boost::system::error_code, T)>
{
};

}

}

namespace boost::asio
{

template <typename CompletionToken, typename Signature>
struct async_result<::eems::as_result_t<CompletionToken>, Signature>
{
    using return_type = typename async_result<CompletionToken, typename ::eems::detail::as_result_signature<Signature>::type>::return_type;

    template <typename Initiation>
    struct init_wrapper
    {
        init_wrapper(Initiation init)
            : initiation_(std::move(init))
        {
        }

        template <typename Handler, typename... Args>
        void operator()(Handler&& handler, Args&&... args)
        {
            std::move(initiation_)(
                ::eems::detail::as_result_handler<std::decay_t<Handler>>(std::forward<Handler>(handler)),
                std::forward<Args>(args)...);
        }

        Initiation initiation_;
    };

    template <typename Initiation, typename RawCompletionToken, typename... Args>
    static return_type initiate(Initiation&& initiation, RawCompletionToken&& token, Args&&... args)
    {
        return async_initiate<CompletionToken,
                              typename ::eems::detail::as_result_signature<Signature>::type>(
            init_wrapper<std::decay_t<Initiation>>(std::forward<Initiation>(initiation)),
            token.token_, std::forward<Args>(args)...);
    }
};

template <typename Handler, typename Executor>
struct associated_executor<::eems::detail::as_result_handler<Handler>, Executor>
{
    typedef typename associated_executor<Handler, Executor>::type type;

    static type get(const ::eems::detail::as_result_handler<Handler>& h,
                    const Executor& ex = Executor()) noexcept
    {
        return associated_executor<Handler, Executor>::get(h.handler_, ex);
    }
};

template <typename Handler, typename Allocator>
struct associated_allocator<::eems::detail::as_result_handler<Handler>, Allocator>
{
    typedef typename associated_allocator<Handler, Allocator>::type type;

    static type get(const ::eems::detail::as_result_handler<Handler>& h,
                    const Allocator& a = Allocator()) noexcept
    {
        return associated_allocator<Handler, Allocator>::get(h.handler_, a);
    }
};

}

#endif
