// Copyright (c) Mateusz Raczynski 2022-2024.
// The software is provided AS IS, with no guarantees for it to work correctly.
// The author doesn't take any responsibility for any damages done.

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <cstdint>
#include <thread>

#include <luavar/luavar.h>
#include <luavar/state.h>

int xyzcalc(int x, int y, int z)
{
    return x * y * z;
}

int xyzcalc2(lua_State *L)
{
    auto lmbd = [](lua_State *L) -> int
    {
        if (!lua_isinteger(L, 1))
            return -1;
        int x = static_cast<int>(luaL_checkinteger(L, 1));
        if (!lua_isinteger(L, 2))
            return -1;
        int y = static_cast<int>(luaL_checkinteger(L, 2));
        if (!lua_isinteger(L, 3))
            return -1;
        int z = static_cast<int>(luaL_checkinteger(L, 3));
        lua_pushinteger(L, xyzcalc(x, y, z));
        return 1;
    };
    auto res = lmbd(L);
    if (res == -1)
    {
        lua_pushfstring(L, "Invalid argument");
        return 1;
    }
    return res;
}

bool waited = false;

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <iostream>

class MyTestListener : public Catch::EventListenerBase
{
public:
    using EventListenerBase::EventListenerBase;

    void testRunEnded(Catch::TestRunStats const &/*testRunStats*/) override
    {
        // std::cout << "All tests have finished running.\n";
        // std::this_thread::sleep_for(std::chrono::milliseconds(60000));
    }
};

// Register the listener
CATCH_REGISTER_LISTENER(MyTestListener);

TEST_CASE("Benchmarks - lua2cpp", "lua2cpp")
{
    SECTION("Lua invalid argument")
    {
        SECTION("Base")
        {
            auto LS = LuaVar::LuaState();
            lua_State *L = LS.Get();

            REQUIRE(L != nullptr);

            // Bind xyzcalc as xyzcalc2
            lua_pushcfunction(L, xyzcalc2);
            lua_setglobal(L, "xyzcalc2");

            BENCHMARK("Lua invalid argument")
            {
                luaL_dostring(L, "res = xyzcalc2(3,\"somestring\",7)");
            };
            lua_getglobal(L, "res");
            REQUIRE(lua_type(L, -1) == LUA_TSTRING);
        }
        SECTION("LuaVar")
        {
            auto LS = LuaVar::LuaState();
            lua_State *L = LS.Get();
            REQUIRE(L != nullptr);
            LuaVar::CppFunction<xyzcalc>("xyzcalc", xyzcalc).Bind(L);
            BENCHMARK("func with 3xint arg, return int")
            {
                luaL_dostring(L, "res = xyzcalc(3,\"somestring\",7)");
            };
            lua_getglobal(L, "res");
            REQUIRE(lua_type(L, -1) == LUA_TSTRING);
        }
    }
    SECTION("Lua three int arguments passed to func")
    {
        SECTION("Base")
        {
            unsigned int i = 0;
            int j = i;
            (void)j;
            auto LS = LuaVar::LuaState();
            lua_State *L = LS.Get();

            REQUIRE(L != nullptr);

            // Bind xyzcalc as xyzcalc2
            lua_pushcfunction(L, xyzcalc2);
            lua_setglobal(L, "xyzcalc2");

            BENCHMARK("func with 3xint arg, return int")
            {
                luaL_dostring(L, "res = xyzcalc2(3,5,7)");
            };
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 105);
            lua_pop(L, -1);
        }
        SECTION("LuaVar")
        {
            auto LS = LuaVar::LuaState();
            lua_State *L = LS.Get();
            REQUIRE(L != nullptr);
            LuaVar::CppFunction<xyzcalc>("xyzcalc", xyzcalc).Bind(L);
            BENCHMARK("func with 3xint arg, return int")
            {
                luaL_dostring(L, "res = xyzcalc(3,5,7)");
            };
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 105);
            lua_pop(L, -1);
        }
    }
}
