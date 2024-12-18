// Copyright (c) Mateusz Raczynski 2022-2024.
// The software is provided AS IS, with no guarantees for it to work correctly.
// The author doesn't take any responsibility for any damages done.

#include <luavar/passing_values.h>

namespace LuaVar
{
    namespace Internal
    {
        template<>
        bool push_result(lua_State *L, double &arg)
        {
            lua_pushnumber(L, arg);
            return true;
        }

        template<>
        bool push_result(lua_State *L, bool &arg)
        {
            lua_pushboolean(L, arg);
            return true;
        }

        template<>
        bool push_result(lua_State *L, int &arg)
        {
            lua_pushinteger(L, arg);
            return true;
        }
    }
}