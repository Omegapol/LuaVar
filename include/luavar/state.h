// Copyright (c) Mateusz Raczynski 2025.
// The software is provided AS IS, with no guarantees for it to work correctly.
// The author doesn't take any responsibility for any damages done.

//
// Created by Mateusz Raczynski on 1/18/2025.
//

#ifndef STATE_H
#define STATE_H
#include <lua.hpp>
#include <luavar/config.h>

namespace LuaVar
{
    LuaVar_API class LuaState
    {
        lua_State *L;

    public:
        LuaState();
        ~LuaState();
        [[nodiscard]] inline lua_State *Get() const
        {
            return L;
        }

        inline operator lua_State *() const
        {
            return L;
        }
    };
}
#endif //STATE_H
