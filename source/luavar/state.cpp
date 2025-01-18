// Copyright (c) Mateusz Raczynski 2025.
// The software is provided AS IS, with no guarantees for it to work correctly.
// The author doesn't take any responsibility for any damages done.

#include <luavar/state.h>

namespace LuaVar
{
    LuaState::LuaState()
    {
        L = luaL_newstate();
    }

    LuaVar::LuaState::~LuaState()
    {
        lua_close(L);
    }
}