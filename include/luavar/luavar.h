// Copyright (c) Mateusz Raczynski 2022-2024.
// The software is provided AS IS, with no guarantees for it to work correctly.
// The author doesn't take any responsibility for any damages done.

#ifndef LUAVAR_LUAVAR_H
#define LUAVAR_LUAVAR_H

#include <cassert>

#include "lua.hpp"
#include <string>
#include <luavar/binding_utils.h>
#include <luavar/type_traits.h>

namespace LuaVar
{
    namespace Internal
    {
        template<LuaFlagsT Flags, typename _RetType>
        struct LuaReturnParser
        {
            using RetType = _RetType;
            using PackedType = std::tuple<RetType>;

            constexpr static int ReturnedValuesCount()
            {
                if constexpr (std::is_same_v<_RetType, void>)
                    return 0;
                return 1;
            }

            inline static RetType GetResults(lua_State *L)
            {
                PackedType res;
                Internal::populate_arguments(L, res);
                return std::get<0>(res);
            }
        };

        template<LuaFlagsT Flags, typename... Args>
        struct LuaReturnParser<Flags, std::tuple<Args...> >
        {
            using RetType = std::tuple<Args...>;
            using PackedType = RetType;

            constexpr static int ReturnedValuesCount()
            {
                if constexpr (Flags::LuaFlagsValue & LuaVariableValueCountReturned)
                {
                    return LUA_MULTRET;
                } else
                {
                    return sizeof...(Args);
                }
            }

            inline static RetType GetResults(lua_State *L)
            {
                PackedType res;
                Internal::populate_arguments(L, res);
                return res;
            }
        };

        template<LuaFlagsT Flags>
        struct LuaReturnParser<Flags, void>
        {
            using RetType = void;
            using PackedType = RetType;

            constexpr static int ReturnedValuesCount()
            {
                //don't bother getting any results if we won't return any
                return 0;
            }

            inline static RetType GetResults(lua_State */*L*/)
            {
            }
        };

        template<typename T, int flags>
        struct Caller
        {
        };

        template<typename RetType, int flags, typename... ArgTypes>
        struct Caller<RetType (*)(ArgTypes...), flags>
        {
            static RetType Call(const std::string &funcName, lua_State *L, ArgTypes... args)
            {
                auto result = lua_getglobal(L, funcName.c_str());
                if constexpr (static_cast<bool>(flags & LuaCallSoftError))
                {
                    if (!lua_isfunction(L, -1))
                    {
                        printf("global %s is not a function\n", funcName.c_str());
                        fflush(stdout);
                        return {};
                    }
                } else
                {
                    assert(result); //"called function that doesn't exist!"
                    luaL_checktype(L, -1, LUA_TFUNCTION);
                }

                using Parser = Internal::LuaReturnParser<LuaFlags<flags>, RetType>;
                std::tuple<ArgTypes...> items(args...);
                Internal::push_arguments(L, items);
                lua_call(L, std::tuple_size_v<decltype(items)>, Parser::ReturnedValuesCount());
                return Parser::GetResults(L);
            }
        };

        template<typename... Args>
        std::tuple<std::string, lua_State *, Args...> prep_args(Args... args)
        {
            return {"", nullptr, args...};
        }

        template<typename Ret, int flags, typename... Types>
        auto decl_impl(const char *name, lua_State *L, std::tuple<Types...> args)
        {
            auto t2 = std::apply<std::tuple<std::string, lua_State *, Types...> (*)(Types...)>(prep_args, args);
            std::get<0>(t2) = name;
            std::get<1>(t2) = L;
            return std::apply(LuaVar::Internal::Caller<Ret (*)(Types...), flags>::Call, t2);
        }
    }


    /**
     * @brief Binds a C++ function to a Lua script environment with the given name.
     *
     * @code
     * #The following example makes SomeFunction C++ function available under `foo` variable in LUA:
     * LuaVar::CppBind<SomeFunction>(L, "foo");
     * @endcode
     *
     * @tparam Func functor instance that will be called when triggered by LUA
     * @param L Pointer to the Lua state.
     * @param name The name to bind the function to in the Lua environment.
     * @return The result of the SimpleBind operation, representing the binding.
     */
    template<auto functor, LuaVarFlags flags = DefaultLuaVarFlags>
    auto CppFunction(char const *name)
    {
        return Internal::StaticBind<functor, flags>(name, functor);
    }

    template<auto functor, LuaVarFlags flags>
    auto CppFunction(char const *name, decltype(functor) /*e*/, flags /**/)
    {
        return Internal::StaticBind<functor, flags>(name, functor);
    }

    template<auto functor>
    auto CppFunction(char const *name, decltype(functor))
    {
        return CppFunction<functor, DefaultLuaVarFlags>(name, functor, DefaultLuaVarFlags{});
    }

    template<Internal::IsLuaVarFunctor Functor, LuaVarFlags flags>
    auto CppFunction(char const *name, Functor &e, flags /**/) ->
        std::enable_if_t<
            Internal::IsCaptureLambda<Functor> && !Internal::IsLuaWrappedCallableDyn<Functor>,
            decltype(Internal::DynamicBind<Functor, flags>(name, e))
        >
    {
        static_assert(!Internal::IsFunctionPointer<Functor>,
                      "Use function as template parameter or wrap it in LuaVar::Callable<>{}");

        return Internal::DynamicBind<Functor, flags>(name, e);
    }

    template<Internal::IsLuaVarFunctor Functor, LuaVarFlags flags>
    auto CppFunction(char const *name, Functor e, flags /**/) ->
        std::enable_if_t<Internal::IsLuaWrappedCallableDyn<Functor>,
            decltype(Internal::DynamicBind<Functor, flags>(name, e))
        >
    {
        static_assert(!Internal::IsFunctionPointer<Functor>,
                      "Use function as template parameter or wrap it in LuaVar::Callable<>{}");

        return Internal::DynamicBind<Functor, flags>(name, e);
    }

    template<typename T, typename K>
    struct HelperWrapResult
    {
        using Res = void;
    };

    template<auto T, LuaVarFlags flags>
    struct HelperWrapResult<Callable<T>, flags>
    {
        using Res = decltype(Internal::StaticBind<Callable<T>::_functor, flags>("name", Callable<T>::_functor));
    };


    template<Internal::IsLuaVarFunctor Functor, LuaVarFlags flags>
    auto CppFunction(char const *name, Functor e, flags /**/) ->
        std::enable_if_t<!Internal::IsDynamicFunctor<Functor>,
            std::conditional_t<Internal::IsLuaWrappedCallable<Functor>,
                typename HelperWrapResult<Functor, flags>::Res,
                decltype(Internal::StaticBind<e, flags>(name, e))>
        >
    {
        static_assert(!Internal::IsFunctionPointer<Functor>,
                      "Use function as template parameter or wrap it in LuaVar::Callable<>{}");

        if constexpr (Internal::IsLuaWrappedCallable<Functor>)
        {
            return Internal::StaticBind<Functor::_functor, flags>(name, Functor::_functor);
        } else
        {
            return Internal::StaticBind<e, flags>(name, e);
        }
    }

    template<Internal::IsCompileTimeFunctor Functor>
    auto CppFunction(char const *name, Functor e) ->
        std::enable_if_t<Internal::IsCompileTimeFunctor<std::remove_reference_t<Functor>>,
            decltype(CppFunction<Functor, DefaultLuaVarFlags>(name, e, DefaultLuaVarFlags{}))
        >
    {
        return CppFunction<Functor, DefaultLuaVarFlags>(name, e, DefaultLuaVarFlags{});
    }
    template<Internal::IsDynamicFunctor Functor>
    auto CppFunction(char const *name, Functor& e) ->
        std::enable_if_t<Internal::IsDynamicFunctor<std::remove_reference_t<Functor>>,
            decltype(CppFunction<Functor, DefaultLuaVarFlags>(name, e, DefaultLuaVarFlags{}))
        >
    {
        return CppFunction<Functor, DefaultLuaVarFlags>(name, e, DefaultLuaVarFlags{});
    }

    template<Internal::IsDynamicFunctor Functor>
    auto CppFunction(char const *name, Functor&& e) ->
        std::enable_if_t<Internal::IsDynamicFunctor<std::remove_reference_t<Functor>>,
            decltype(CppFunction<Functor, DefaultLuaVarFlags>(name, e, DefaultLuaVarFlags{}))
        >
    {
        return CppFunction<Functor, DefaultLuaVarFlags>(name, e, DefaultLuaVarFlags{});
    }

    /***
    * Define LUA function that can be called from C++.
    * The function existence and correctness is NOT verified until it is triggered.
    *
    * @code
    * auto FunctionName = LuaVar::LuaFunction<bool(*)(int, int)>("lua_function_name");
    * @endcode
    */
    template<typename Functor, LuaFlagsT FlagsT = LuaFlags<LuaCallDefaultMode> >
    class LuaFunction;

    /**
     * @class LuaFunction
     * @brief A wrapper for Lua functions, providing an interface to call Lua functions from C++.
     *
     * This templated class associates a function name with a C++ function and provides
     * mechanisms to invoke the function in Lua using the specified arguments. It can be
     * instantiated with a Lua function name alone or with a Lua function name and a corresponding
     * C++ callable Functor (used as a signature prototype).
     *
     * The LUA function existence and correctness is NOT verified until it is triggered.
     *
     * @code
     * auto FunctionName = LuaVar::LuaFunction<bool(*)(int, int)>("lua_function_name");
     *
     * #calling the function
     * bool res = FunctionName(5, 10);
     * @endcode
     *
     * @tparam Ret The return type of the Lua function.
     * @tparam Args The parameter types of the Lua function.
     */
    template<typename Ret, typename... Args, LuaFlagsT FlagsT>
    class LuaFunction<Ret (*)(Args...), FlagsT>
    {
        const char *name = "";
        using FunctorF = Ret (*)(Args...);

    public:
        /**
         * @brief Constructs a LuaFunction object with a specified name.
         *
         * @param name The name of the Lua function as a constant character pointer.
         * @return A constexpr LuaFunction object initialized with the provided name.
         */
        explicit constexpr LuaFunction(char const *name): name(name)
        {
        }

        /**
         * @brief Constructs a LuaFunction object with a name and a function pointer.
         *
         * This constructor initializes a LuaFunction with a name and a functor function
         * pointer. The function pointer itself is not stored nor used in runtime, it is only used compile time to detect function signature.
         *
         * @param name A constant character pointer representing the name of the Lua function.
         * @param functor A function pointer to the Lua function implementation that takes
         *        a variable number of template arguments and returns a value of type Ret.
         */
        explicit constexpr LuaFunction(char const *name, Ret (*functor)(Args...)): name(name)
        {
            (void) functor;
        }

        /**
         * @brief Constructs a LuaFunction object with a specified name.
         *
         * @param name The name of the Lua function as a constant character pointer.
         * @return A constexpr LuaFunction object initialized with the provided name.
         */
        explicit constexpr LuaFunction(char const *name, FlagsT flags_): name(name)
        {
            (void) flags_;
        }

        /**
         * @brief Constructs a LuaFunction object with a name and a function pointer.
         *
         * This constructor initializes a LuaFunction with a name and a functor function
         * pointer. The function pointer itself is not stored nor used in runtime, it is only used compile time to detect function signature.
         *
         * @param name A constant character pointer representing the name of the Lua function.
         * @param functor A function pointer to the Lua function implementation that takes
         *        a variable number of template arguments and returns a value of type Ret.
         */
        explicit constexpr LuaFunction(char const *name, Ret (*functor)(Args...), FlagsT flags): name(name)
        {
            (void) functor;
            (void) flags;
        }

        template<typename LuaFlags>
        constexpr LuaFunction<FunctorF, LuaFlags> Flags(LuaFlags /*luaFlags*/  = {})
        {
            return LuaFunction<FunctorF, LuaFlags>(name);
        }

        /**
         * Calls a Lua function with the specified arguments and returns the result.
         *
         * @param L The Lua state within which the function call will be executed.
         * @param args The arguments to be passed to the Lua function.
         * @return The result of the Lua function call, with the type determined by the template type `Ret`.
         */
        Ret Call(lua_State *L, Args... args)
        {
            return Internal::decl_impl<Ret, FlagsT::LuaFlagsValue>(name, L, std::make_tuple(args...));
        }

        /**
         * Overloaded function call operator to invoke the Call function with the given arguments.
         *
         * @param L A pointer to the lua_State representing the Lua state.
         * @param args The variadic arguments to be passed to the Call function.
         * @return The result of the Call function of type Ret.
         */
        Ret operator()(lua_State *L, Args... args)
        {
            return Call(L, args...);
        }
    };

    template<Internal::IsFunctionPointer Functor>
    LuaFunction(char const *name, Functor e) -> LuaFunction<Functor>;
    template<Internal::IsFunctionPointer Functor, LuaFlagsT flags>
    LuaFunction(char const *name, Functor e, flags f_) -> LuaFunction<Functor, flags>;
}



#endif //LUAVAR_LUAVAR_H
