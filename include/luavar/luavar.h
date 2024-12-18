// Copyright (c) Mateusz Raczynski 2022-2024.
// The software is provided AS IS, with no guarantees for it to work correctly.
// The author doesn't take any responsibility for any damages done.

#ifndef LUAVAR_LUAVAR_H
#define LUAVAR_LUAVAR_H

#include <cassert>

#include "lua.hpp"
#include <string>
#include <luavar/passing_values.h>

#include <functional>
#include <concepts>

namespace LuaVar
{
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

    namespace Internal
    {
        template<::std::size_t I = 0,
            typename... Tp>
        inline typename ::std::enable_if<I == sizeof...(Tp), bool>::type
        populate_arguments(lua_State */*L*/, ::std::tuple<Tp...> &/*t*/)
        {
            return true;
        }

        template<::std::size_t I = 0,
            typename... Tp>
        inline typename ::std::enable_if<I < sizeof...(Tp), bool>::type
        populate_arguments(lua_State *L, ::std::tuple<Tp...> &t)
        {
            using TupleType = ::std::tuple<Tp...>;
            return Argument<std::tuple_element_t<I, TupleType> >::template get_argument<I + 1>(L, ::std::get<I>(t)) &&
                   populate_arguments<I + 1, Tp...>(L, t);
        }

        template<::std::size_t I = 0, typename... Tp>
        inline typename ::std::enable_if<I == sizeof...(Tp), void>::type
        push_arguments(lua_State */*L*/, ::std::tuple<Tp...> &/*t*/)
        {
        }

        template<::std::size_t I = 0, typename... Tp>
        inline typename ::std::enable_if<I < sizeof...(Tp), void>::type
        push_arguments(lua_State *L, ::std::tuple<Tp...> &t)
        {
            push_result(L, ::std::get<I>(t));
            push_arguments<I + 1, Tp...>(L, t);
        }

#pragma region CallableTraits //todo: move this to separate header and make it configurable
        // For generic types that are functors, delegate to its 'operator()'
        template<typename T>
        struct function_traits
                : public function_traits<decltype(&T::operator())>
        {
        };

        // for pointers to member function
        template<typename ClassType, typename ReturnType, typename... Args>
        struct function_traits<ReturnType(ClassType::*)(Args...) const>
        {
            //enum { arity = sizeof...(Args) };
            typedef std::function<ReturnType (Args...)> f_type;
            using arguments_type = std::tuple<Args...>;
        };

        // for pointers to member function
        template<typename ClassType, typename ReturnType, typename... Args>
        struct function_traits<ReturnType(ClassType::*)(Args...)>
        {
            typedef std::function<ReturnType (Args...)> f_type;
            using arguments_type = std::tuple<Args...>;
        };

        // for function pointers
        template<typename ReturnType, typename... Args>
        struct function_traits<ReturnType (*)(Args...)>
        {
            typedef std::function<ReturnType (Args...)> f_type;
            using arguments_type = std::tuple<Args...>;
        };

        template<typename L>
        typename function_traits<L>::f_type make_function(L l)
        {
            return (typename function_traits<L>::f_type) (l);
        }


#pragma endregion
        template<typename T, int flags = LuaCallDefaultMode>
        struct FunctorDescriptor
        {
            // using Traits = CallableTraits<decltype(&T::operator())>;
            typedef function_traits<T> Traits;
            using ReturnType = typename Traits::f_type::result_type;
            // using Arguments = typename Traits::test_type;
            using Arguments = typename Traits::arguments_type;
            static const int CallMode = flags;

            static int call(lua_State *L, T functor)
            {
                Arguments items;
                // gather arguments from lua stack and push them into `items` structure
                auto b = populate_arguments(L, items);

                if (!b)
                {
                    if constexpr (flags & LuaCallSoftError)
                    {
                        printf("Invalid arguments provided\n");
                        fflush(stdout);
                        return 0;
                    } else
                    {
                        // luaL_error(L, "Invalid arguments");
                        return -1;
                    }
                }

                if constexpr (std::is_same_v<ReturnType, void>)
                {
                    // Functor is of void return type, just call it
                    std::apply(functor, items);
                } else
                {
                    ReturnType res;
                    // Functor is of non-void return type, call and assign result
                    res = std::apply(functor, items);
                    push_result(L, res);
                }
                return 1;
            }
        };


        // template<typename Ret, int flags, typename... Types>
        // struct FunctorDescriptor<Ret (*)(Types...), flags>
        // {
        //     using ReturnType = Ret;
        //     using Arguments = std::tuple<Types...>;
        //     static const int CallMode = flags;
        //
        //     static int call(lua_State *L, Ret (*functor)(Types...))
        //     {
        //         Arguments items;
        //         // gather arguments from lua stack and push them into `items` structure
        //         auto b = populate_arguments(L, items);
        //
        //         if (!b)
        //         {
        //             if constexpr (flags & LuaCallSoftError)
        //             {
        //                 printf("Invalid arguments provided\n");
        //                 fflush(stdout);
        //                 return 0;
        //             } else
        //             {
        //                 // luaL_error(L, "Invalid arguments");
        //                 return -1;
        //             }
        //         }
        //
        //         if constexpr (std::is_same_v<ReturnType, void>)
        //         {
        //             // Functor is of void return type, just call it
        //             std::apply(functor, items);
        //         } else
        //         {
        //             ReturnType res;
        //             // Functor is of non-void return type, call and assign result
        //             res = std::apply(functor, items);
        //             push_result(L, res);
        //         }
        //         return 1;
        //     }
        // };


        template<typename T, T functor, typename K>
        struct Binder
        {
            static void bind(lua_State *L, const std::string &name)
            {
                // create lambda that handles actual call into functor
                auto wrapper = [](lua_State *L)
                {
                    int res = K::call(L, functor);
                    if (res == -1)
                    {
                        lua_pushfstring(L, "Invalid arguments");
                        // luaL_error(L, "Invalid arguments"); //todo: investigate if luaL_error can be used safely here
                        return 1;
                    }
                    return res;
                };

                // assign the lambda to target name
                lua_pushcfunction(L, wrapper);
                lua_setglobal(L, name.c_str());
            }
        };

        template<typename T, T functor, int flags>
        void SimpleBind(lua_State *L, const std::string &name)
        {
            using FuncType = decltype(functor);
            Binder<decltype(functor), functor, FunctorDescriptor<FuncType, flags> >::bind(L, name);
        };
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
    template<auto Func, int flags = LuaCallDefaultMode>
    auto CppBind(lua_State *L, const char *name)
    {
        // return Internal::SimpleBind<decltype(Func), Func, flags>(L, name);
        using FuncType = decltype(Func);
        Internal::Binder<FuncType, Func, Internal::FunctorDescriptor<FuncType, flags> >::bind(L, name);
    }

    template<typename T, int flags>
    struct Caller
    {
    };
#pragma region istuple //todo: move this to separate header and make it configurable
    template<typename T>
    struct is_tuple : std::false_type
    {
    };

    template<typename... Ts>
    struct is_tuple<std::tuple<Ts...> > : std::true_type
    {
    };
#pragma endregion

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

            std::tuple<ArgTypes...> items(args...);
            Internal::push_arguments(L, items);
            if constexpr (flags & LuaVariableValueCountReturned)
            {
                // Handle variable value count returned from LUA function
                static_assert(is_tuple<RetType>::value, "Provided signature must return std::tuple<>");
                // int stack_diff = lua_gettop(L);
                lua_call(L, std::tuple_size<decltype(items)>::value, LUA_MULTRET);
                RetType res;
                Internal::populate_arguments(L, res);
                return res;
            } else
            {
                if constexpr (is_tuple<RetType>::value)
                {
                    lua_call(L, std::tuple_size<decltype(items)>::value, std::tuple_size_v<RetType>);
                    // if return type was tuple, try to pick up all the values
                    RetType res;
                    Internal::populate_arguments(L, res);
                    return res;
                } else
                {
                    lua_call(L, std::tuple_size<decltype(items)>::value, 1);
                    // pick up single value returned
                    std::tuple<RetType> res;
                    Internal::populate_arguments(L, res);
                    return std::get<0>(res);
                }
            }
        }
    };

    template<int _flags>
    class LuaFlags
    {
    public:
        static constexpr int LuaFlagsValue = _flags;
        LuaFlags() = default;
    };

    template<typename T>
    concept LuaFlagsT = requires { T::LuaFlagsValue; };

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
            return decl_impl<Ret, FlagsT::LuaFlagsValue>(name, L, std::make_tuple(args...));
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

    // Concept to check if a type is a function pointer
    template<typename T>
    concept IsFunctionPointer = std::is_pointer_v<T> && std::is_function_v<std::remove_pointer_t<T> >;

    template<IsFunctionPointer Functor>
    LuaFunction(char const *name, Functor e) -> LuaFunction<Functor>;
    template<IsFunctionPointer Functor, LuaFlagsT flags>
    LuaFunction(char const *name, Functor e, flags f_) -> LuaFunction<Functor, flags>;
}

#define LuaBind(L, functor, name) LuaVar::Internal::SimpleBind<decltype(functor), functor, LuaVar::LuaCallDefaultMode>(L, name)
#define LuaBindV(L, functor, name, flag) LuaVar::Internal::SimpleBind<decltype(functor), functor, flag>(L, name)
#define LuaCall(L, Signature, name, ...) LuaVar::Caller<Signature, LuaVar::LuaCallDefaultMode>::Call(name, L, ##__VA_ARGS__)
#define LuaCallV(L, Signature, name, flag, ...) LuaVar::Caller<Signature, flag>::Call(name, L, __VA_ARGS__)
#define LuaBindA(L, functor_name) LuaBind(L, functor_name, #functor_name)


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
    return std::apply(LuaVar::Caller<Ret (*)(Types...), flags>::Call, t2);
}

#define LuaDeclareV(name, Signature, flag) LuaVar::Internal::FunctorDescriptor<Signature>::ReturnType name \
(lua_State* L, LuaVar::Internal::FunctorDescriptor<Signature>::Arguments args) {                                            \
    return decl_impl<LuaVar::Internal::FunctorDescriptor<Signature>::ReturnType, flag>(#name, L, args); \
}

#define LuaDeclare(name, Signature) LuaDeclareV(name, Signature, LuaVar::LuaCallDefaultMode)

#define LuaDeclareB(Ret, name, ...) \
template<typename ...Args> \
Ret name (lua_State*L, Args... args) {           \
    std::string _name = #name; \
    std::tuple<Args...> arguments{args...};      \
    static_assert(std::is_same<decltype(arguments), std::tuple<__VA_ARGS__>>(), "Invalid argument types, should be: "#__VA_ARGS__);                                \
    return decl_impl<Ret, LuaVar::LuaCallDefaultMode>(_name.c_str(), L, arguments);         \
}

#define LuaDeclareC(Ret, name, ...) \
Ret name (lua_State*L, std::tuple<__VA_ARGS__> args) {           \
    std::string _name = #name; \
    return decl_impl<Ret, LuaVar::LuaCallDefaultMode>(_name.c_str(), L, args);         \
}

#define LuaDeclareD(Ret, name, ...) \
Ret name(lua_State* L, __VA_ARGS__) { \
    std::string _name = #name; \
    return decl_impl<Ret, LuaVar::LuaCallDefaultMode>(_name.c_str(), L, std::make_tuple(__VA_ARGS__)); \
}

#endif //LUAVAR_LUAVAR_H
