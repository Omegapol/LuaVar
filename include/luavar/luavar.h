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


    template<auto functor>
        requires requires { functor(); }
    struct Callable
    {
        using CallableType = decltype(functor);
        inline static const auto _functor = functor;
    };


    template<typename T>
    struct CallableDyn
    {
        const T _functor;

        CallableDyn(const T _functor) : _functor(_functor)
        {
        };
    };

    template<typename T>
    concept LuaVarFlags = requires(T t, LuaVar::LuaCallFlag flag)
    {
        T::LuaFlagsValue;
        { T::LuaFlagsValue } -> std::same_as<const int &>;
        { T::IsSet(flag) } -> std::same_as<bool>;
    };

    typedef LuaFlags<LuaCallDefaultMode> DefaultLuaVarFlags;

    namespace Internal
    {
        template<typename T>
        concept IsFunctionPointer = std::is_pointer_v<T> && std::is_function_v<std::remove_pointer_t<T> >;

        template<typename T>
        concept IsCaptureLambda = !std::is_standard_layout_v<T> ||
                                  !std::is_trivial_v<T>;

        template<typename T>
        concept NonCaptureLambda = !IsCaptureLambda<T>;

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
        //todo do proper check that it is some sort of callable

        template<typename T>
        concept IsDynamicFunctor = IsLuaWrappedCallableDyn<T> || IsCaptureLambda<T>;

        template<typename T>
        concept IsFunctor = IsDynamicFunctor<T> || IsCompileTimeFunctor<T>;

        template<typename Functor>
        using ForwardFunctor = std::conditional_t<IsDynamicFunctor<Functor>, Functor &, Functor>;

        template<typename Functor>
        struct FunctorAdapter
        {
            using FunctorType = Functor;

            static Functor &GetFunctor(Functor &functor) { return functor; }
        };


        template<typename Functor>
        struct FunctorAdapter<CallableDyn<Functor> >
        {
            using FunctorType = Functor;

            static const FunctorType &GetFunctor(CallableDyn<Functor> &complexFunctor)
            {
                return complexFunctor._functor;
            }
        };


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
        //  handles also non-capturing lambdas
        template<typename ClassType, typename ReturnType, typename... Args>
        struct function_traits<ReturnType(ClassType::*)(Args...) const>
        {
            //enum { arity = sizeof...(Args) };
            typedef std::function<ReturnType (Args...)> f_type;
            using arguments_type = std::tuple<Args...>;
        };

        // for pointers to member function
        // template<typename ClassType, typename ReturnType, typename... Args>
        // struct function_traits<ReturnType(ClassType::*)(Args...)>
        // {
        //     typedef std::function<ReturnType (Args...)> f_type;
        //     using arguments_type = std::tuple<Args...>;
        // };
        //
        // for function pointers
        template<typename ReturnType, typename... Args>
        struct function_traits<ReturnType (*)(Args...)>
        {
            typedef std::function<ReturnType (Args...)> f_type;
            using arguments_type = std::tuple<Args...>;
        };

#pragma endregion
        template<typename T, LuaVarFlags flags>
        struct FunctorDescriptor
        {
        };

        template<IsFunctor T, LuaVarFlags flags>
        struct FunctorDescriptor<T, flags>
        {
            // using Traits = CallableTraits<decltype(&T::operator())>;
            // typedef function_traits<T> Traits;
            typedef function_traits<std::remove_reference_t<T> > Traits;
            using ReturnType = typename Traits::f_type::result_type;
            using Arguments = typename Traits::arguments_type;
            using FunctorType = std::conditional_t<IsDynamicFunctor<T>, T &, T>;
            // use reference for dynamic functors to avoid copy

            static int call(lua_State *L, FunctorType functor)
            {
                Arguments items;
                // gather arguments from lua stack and push them into `items` structure
                auto b = populate_arguments(L, items);

                if (!b)
                {
                    if constexpr (flags::IsSet(LuaCallSoftError))
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

        static int GetAvailableId()
        {
            static int id = 0;
            return ++id;
        }

        template<IsDynamicFunctor Functor, LuaVarFlags flags>
        class DynamicBind
        {
            using Flags = flags;

            using Adapter = FunctorAdapter<Functor>;
            using ActualFunctorType = typename Adapter::FunctorType;
            using K = FunctorDescriptor<ActualFunctorType, flags>;

            using FunctorArgType = ForwardFunctor<Functor>;
            using ActualFunctorArgType = ForwardFunctor<ActualFunctorType>;

            const char *_name;
            ActualFunctorType functor;

            struct Wrapper
            {
                ActualFunctorType functor;

                explicit Wrapper(ActualFunctorArgType functor) : functor(functor)
                {
                }

                static int Clear(lua_State *L)
                {
                    Wrapper *obj = *reinterpret_cast<Wrapper **>(lua_touserdata(L, 1));
                    delete obj;
                    return 0;
                }
            };

            static void bind(lua_State *L, const std::string &name, ActualFunctorArgType functor)
            {
                // create lambda that handles actual call into functor
                auto wrapper = [](lua_State *L)
                {
                    auto *capture = *reinterpret_cast<Wrapper **>(lua_touserdata(L, lua_upvalueindex(1)));

                    int res = K::call(L, capture->functor);
                    if (res == -1)
                    {
                        lua_pushfstring(L, "Invalid arguments");
                        // luaL_error(L, "Invalid arguments"); //todo: investigate if luaL_error can be used safely here
                        return 1;
                    }
                    return res;
                };

                // Allocate userdata
                auto **ud = static_cast<Wrapper **>(lua_newuserdata(L, sizeof(Wrapper*)));
                *ud = new Wrapper(functor);

                // Create and set the metatable
                static auto id = GetAvailableId();
                auto meta_name = std::string("Function#") + std::to_string(id);
                if (luaL_newmetatable(L, meta_name.c_str()))
                {
                    // Ensure metatable is created only once
                    lua_pushcfunction(L, Wrapper::Clear);
                    lua_setfield(L, -2, "__gc"); // Set __gc field in the metatable
                }
                lua_setmetatable(L, -2);

                // assign the lambda to target name
                lua_pushcclosure(L, wrapper, 1);
                lua_setglobal(L, name.c_str());
            }

        public:
            constexpr DynamicBind(char const *name, FunctorArgType functor) : _name(name),
                                                                              functor(Adapter::GetFunctor(functor))
            {
            }

            void Bind(lua_State *L)
            {
                bind(L, _name, functor);
            }
        };


        template<auto functor, LuaVarFlags flags>
        class StaticBind
        {
            using Flags = flags;
            using FunctorType = decltype(functor);
            const char *_name;

        public:
            constexpr StaticBind(char const *name, FunctorType /*functor*/) : _name(name)
            {
            }

            void Bind(lua_State *L)
            {
                using K = Internal::FunctorDescriptor<FunctorType, flags>;
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
                lua_setglobal(L, _name);
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

    // /**
    //  * @brief Binds a C++ function to a Lua script environment with the given name.
    //  *
    //  * @code
    //  * #The following example makes SomeFunction C++ function available under `foo` variable in LUA:
    //  * LuaVar::CppBind<SomeFunction>(L, "foo");
    //  * @endcode
    //  *
    //  * @tparam Func functor instance that will be called when triggered by LUA
    //  * @param L Pointer to the Lua state.
    //  * @param name The name to bind the function to in the Lua environment.
    //  * @return The result of the SimpleBind operation, representing the binding.
    //  */
    // template<auto Func, int flags = LuaCallDefaultMode>
    // auto CppBind(lua_State *L, const char *name)
    // {
    //     // return Internal::SimpleBind<decltype(Func), Func, flags>(L, name);
    //     using FuncType = decltype(Func);
    //     Internal::Binder<FuncType, Func, Internal::FunctorDescriptor<FuncType, flags> >::bind(L, name);
    // }


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

    template<Internal::IsFunctor Functor, LuaVarFlags flags>
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

    template<Internal::IsFunctor Functor, LuaVarFlags flags>
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


    template<Internal::IsFunctor Functor, LuaVarFlags flags>
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

    template<Internal::IsFunctor Functor>
    auto CppFunction(char const *name, Functor e) ->
        std::enable_if_t<Internal::NonCaptureLambda<std::remove_reference_t<Functor>>,
            decltype(CppFunction<Functor, DefaultLuaVarFlags>(name, e, DefaultLuaVarFlags{}))
        >
    {
        return CppFunction<Functor, DefaultLuaVarFlags>(name, e, DefaultLuaVarFlags{});
    }
    template<Internal::IsCaptureLambda Functor>
    auto CppFunction(char const *name, Functor& e) ->
        std::enable_if_t<Internal::IsCaptureLambda<std::remove_reference_t<Functor>>,
            decltype(CppFunction<Functor, DefaultLuaVarFlags>(name, e, DefaultLuaVarFlags{}))
        >
    {
        return CppFunction<Functor, DefaultLuaVarFlags>(name, e, DefaultLuaVarFlags{});
    }

    template<Internal::IsCaptureLambda Functor>
    auto CppFunction(char const *name, Functor&& e) ->
        std::enable_if_t<Internal::IsCaptureLambda<std::remove_reference_t<Functor>>,
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
