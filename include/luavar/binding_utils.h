// Copyright (c) Mateusz Raczynski 2022-2024.
// The software is provided AS IS, with no guarantees for it to work correctly.
// The author doesn't take any responsibility for any damages done.

#ifndef LUAVAR_PASSING_VALUES_H
#define LUAVAR_PASSING_VALUES_H

#include <lua.hpp>
#include <string>
#include <luavar/config.h>
#include <luavar/type_traits.h>

namespace LuaVar
{
    namespace Internal
    {
        struct none_type
        {
        };

        template<typename ArgType>
        struct Argument
        {
            template<int Index>
            static bool get_argument(lua_State */*L*/, ArgType &/*arg*/ = {})
            {
                return false;
            }
        };

        template<>
        template<int Index>
        inline bool Argument<int>::get_argument(lua_State *L, int &arg)
        {
            if (!lua_isnumber(L, Index))
            {
                return false;
            }
            arg = static_cast<int>(luaL_checkinteger(L, Index));
            return true;
        }


        template<>
        template<int Index>
        inline bool Argument<double>::get_argument(lua_State *L, double &arg)
        {
            if (!lua_isnumber(L, Index))
            {
                return false;
            }
            arg = luaL_checknumber(L, Index);
            return true;
        }

        template<>
        template<int Index>
        inline bool Argument<std::string>::get_argument(lua_State *L, std::string &arg)
        {
            if (!lua_isstring(L, Index))
            {
                return false;
            }
            auto s = luaL_checkstring(L, Index);
            arg = s;
            return true;
        }

        template<typename ArgType>
        bool push_result(lua_State *L, ArgType &arg);

        template <typename ... Args, std::size_t... Indices>
        constexpr bool push_tuple_result(lua_State *L, std::tuple<Args...> &arg, std::index_sequence<Indices...>) {
            return ((push_result(L, std::get<Indices>(arg))) && ...);
        }

        template<typename ... Args>
        bool push_result(lua_State *L, std::tuple<Args...> &arg)
        {
            return push_tuple_result(L, arg, std::make_index_sequence<std::tuple_size_v<std::tuple<Args...>>>{});
        }

        template<>
        LuaVar_API bool push_result(lua_State *L, std::string &arg);
        template<>
        LuaVar_API bool push_result(lua_State *L, const char* &arg);
        template<>
        LuaVar_API bool push_result(lua_State *L, double &arg);
        template<>
        LuaVar_API bool push_result(lua_State *L, bool &arg);
        template<>
        LuaVar_API bool push_result(lua_State *L, int &arg);


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


        template<typename T, LuaVarFlags flags>
        struct FunctorDescriptor
        {
        };

        template<IsLuaVarFunctor T, LuaVarFlags flags>
        struct FunctorDescriptor<T, flags>
        {
            typedef type_traits<std::remove_reference_t<T> > Traits;
            using ReturnType = typename Traits::f_type::result_type;
            using Arguments = typename Traits::arguments_type;
            // use reference for dynamic functors to avoid copy
            using FunctorType = std::conditional_t<IsDynamicFunctor<T>, T &, T>;

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
                    return 0;
                } else
                {
                    // Functor is of non-void return type, call and assign result
                    ReturnType res = std::apply(functor, items);
                    push_result(L, res);
                    if constexpr (IsTuple<ReturnType>)
                        return std::tuple_size_v<ReturnType>;
                    else
                        return 1;
                }
            }
        };


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

        public:

            static void PushFunctor(lua_State *L, ActualFunctorArgType functor)
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
            }

            constexpr DynamicBind(char const *name, FunctorArgType functor) : _name(name),
                                                                              functor(Adapter::GetFunctor(functor))
            {
            }

            void Bind(lua_State *L)
            {
                PushFunctor(L, functor);
                lua_setglobal(L, _name);
            }
        };

        template<typename ArgType>
        bool push_result(lua_State *L, ArgType &arg)
        {
            if constexpr (IsFunctor<ArgType>)
            {
                auto callable_dyn = CallableDyn{arg};
                DynamicBind<CallableDyn<ArgType>, DefaultLuaVarFlags>{"", callable_dyn}.PushFunctor(L, arg);
                return true;
            } else
            {
                static_assert(false, "not supported type");
                return false;
            }
        }
    }
}

#endif //LUAVAR_PASSING_VALUES_H
