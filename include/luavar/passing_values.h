// Copyright (c) Mateusz Raczynski 2022-2024.
// The software is provided AS IS, with no guarantees for it to work correctly.
// The author doesn't take any responsibility for any damages done.

#ifndef LUAVAR_PASSING_VALUES_H
#define LUAVAR_PASSING_VALUES_H

#include <lua.hpp>
#include <string>
#include <luavar/config.h>

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
        bool push_result(lua_State *L, ArgType &arg)
        {
            return false;
        }

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
    }
}

#endif //LUAVAR_PASSING_VALUES_H
