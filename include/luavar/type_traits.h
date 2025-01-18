// Copyright (c) Mateusz Raczynski 2025.
// The software is provided AS IS, with no guarantees for it to work correctly.
// The author doesn't take any responsibility for any damages done.

#ifndef META_H
#define META_H

#include <functional>
#include <concepts>

namespace LuaVar {


    /**
     * @enum LuaCallFlag
     * @brief Enumeration representing various flags for configuring Lua function calls.
     *
     * This enumeration defines a set of flags that can be used to modify the behavior
     * of a Lua function call. Each flag represents a distinct option or mode that can
     * be applied individually or in combination.
     *
     * @var LuaCallDefaultMode
     * Default mode for Lua function calls with no special configuration.
     *
     * @var LuaCallSoftError
     * Indicates that errors should be handled softly without halting execution.
     *
     * @var LuaParamTypeCheck
     * Enables type checking of parameters passed to the Lua function.
     */
    enum LuaCallFlag
    {
        LuaCallDefaultMode = 0b0000,
        LuaCallSoftError = 0b1000,
        LuaParamTypeCheck = 0b0100, //todo: not used currently
        LuaVariableValueCountReturned = 0b00100
    };

    template<int _flags>
    class LuaFlags
    {
    public:
        static constexpr int LuaFlagsValue = _flags;
        LuaFlags() = default;

        static constexpr bool IsSet(LuaCallFlag flag)
        {
            return (_flags & static_cast<int>(flag)) != 0;
        }

        static constexpr auto Set(LuaCallFlag flag)
        {
            return LuaFlags<_flags | static_cast<int>(flag)>{};
        }
    };

    template<typename T>
    concept LuaFlagsT = requires { T::LuaFlagsValue; };

    template<typename T>
    concept LuaVarFlags = requires(T t, LuaVar::LuaCallFlag flag)
    {
        T::LuaFlagsValue;
        { T::LuaFlagsValue } -> std::same_as<const int &>;
        { T::IsSet(flag) } -> std::same_as<bool>;
    };

    typedef LuaFlags<LuaCallDefaultMode> DefaultLuaVarFlags;

namespace Internal {

    // For generic types that are functors, delegate to its 'operator()'
    template<typename T, typename = void>
    struct type_traits
    {
    };

    // for pointers to member function
    //  handles also non-capturing lambdas
    template<typename ClassType, typename ReturnType, typename... Args>
    struct type_traits<ReturnType(ClassType::*)(Args...) const>
    {
        //enum { arity = sizeof...(Args) };
        typedef std::function<ReturnType (Args...)> f_type;
        using arguments_type = std::tuple<Args...>;
    };

    // for pointers to member function
    template<typename ClassType, typename ReturnType, typename... Args>
    struct type_traits<ReturnType(ClassType::*)(Args...)>
    {
        typedef std::function<ReturnType (Args...)> f_type;
        using arguments_type = std::tuple<Args...>;
    };

    // for function pointers
    template<typename ReturnType, typename... Args>
    struct type_traits<ReturnType (*)(Args...)>
    {
        typedef std::function<ReturnType (Args...)> f_type;
        using arguments_type = std::tuple<Args...>;
    };

    template<typename T>
    struct type_traits<T, std::void_t<decltype(&T::operator())> >
            : public type_traits<decltype(&T::operator())>
    {
        using f_type = typename type_traits<decltype(&T::operator())>::f_type;
        using arguments_type = typename type_traits<decltype(&T::operator())>::arguments_type;
    };

    template <typename T>
    concept IsFunctor = requires(T t) {
        { !std::is_same_v<typename type_traits<T>::f_type, void> };
    };

    template<typename T>
    concept IsFunctionPointer = std::is_pointer_v<T> && std::is_function_v<std::remove_pointer_t<T> >;

    template<typename T>
    concept IsCaptureLambda = (!std::is_standard_layout_v<T> ||
                              !std::is_trivial_v<T>) && IsFunctor<T>;

    template<typename T>
    concept NonCaptureLambda = !IsCaptureLambda<T> && IsFunctor<T>;

    template<typename T>
    concept IsTuple = requires
    {
        typename std::tuple_size<T>::type;
    };

}
    template<auto functor>
        requires Internal::IsFunctor<decltype(functor)>
    struct Callable
{
    using CallableType = decltype(functor);
    static constexpr auto _functor = functor;
};


    template<typename T>
    struct CallableDyn
    {
        const T _functor;

        CallableDyn(const T _functor) : _functor(_functor)
        {
        };
    };

    namespace Internal
    {

        template<typename T>
        struct IsLuaWrappedCallableT : std::false_type
        {
        };

        template<auto T>
        struct IsLuaWrappedCallableT<Callable<T> > : std::true_type
        {
        };

        template<typename T>
        concept IsLuaWrappedCallable = (IsLuaWrappedCallableT<T>{} == true);

        template<typename T>
        struct IsLuaWrappedCallableDynT : std::false_type
        {
        };

        template<typename T>
        struct IsLuaWrappedCallableDynT<CallableDyn<T> > : std::true_type
        {
        };

        template<typename T>
        concept IsLuaWrappedCallableDyn = (IsLuaWrappedCallableDynT<T>{} == true);

        template<typename T>
        concept IsCompileTimeFunctor = (IsLuaWrappedCallable<T> == true) || NonCaptureLambda<T>;

        template<typename T>
        concept IsDynamicFunctor = IsLuaWrappedCallableDyn<T> || IsCaptureLambda<T>;

        template<typename T>
        concept IsLuaVarFunctor = IsDynamicFunctor<T> || IsCompileTimeFunctor<T>;
    }
}
#endif //META_H
