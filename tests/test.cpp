// Copyright (c) Mateusz Raczynski 2022-2024.
// The software is provided AS IS, with no guarantees for it to work correctly.
// The author doesn't take any responsibility for any damages done.

// #define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

// #ifdef WIN32
// #define _CRTDBG_MAP_ALLOC
// #include <crtdbg.h>
//
// struct CrtCheckMemory
// {
//     _CrtMemState state1;
//     _CrtMemState state2;
//     _CrtMemState state3;
//
//     CrtCheckMemory()
//     {
//         _CrtMemCheckpoint(&state1);
//     }
//
//     ~CrtCheckMemory()
//     {
//         _CrtMemCheckpoint(&state2);
//         // using google test you can just do this.
//         // EXPECT_EQ(0,_CrtMemDifference( &state3, &state1, &state2));
//         // else just do this to dump the leaked blocks to stdout.
//         if (_CrtMemDifference(&state3, &state1, &state2))
//             _CrtMemDumpStatistics(&state3);
//     }
// };
// #else
// struct CrtCheckMemory {};
// #endif

#include <catch2/catch_test_macros.hpp>
#include <cstdint>

#include "luavar/luavar.h"


int foo0()
{
    return 1;
}

int foo1(int x)
{
    return x;
}

int foo1str(std::string x)
{
    return static_cast<int>(x.length());
}

int foo2(int x, int y)
{
    return x * y;
}

int xyzcalc(int x, int y, int z)
{
    return x * y * z;
}

static int noValueCalled = 0;

void NoValueFoo(int x)
{
    noValueCalled = x;
}

TEST_CASE("Meta tests")
{
    lua_State *L = luaL_newstate();
    SECTION("Test populating arguments")
    {
        SECTION("Multiple ints")
        {
            std::tuple<int, int> args{1, 2};
            lua_pushinteger(L, 100);
            lua_pushinteger(L, 200);
            LuaVar::Internal::populate_arguments(L, args);
            REQUIRE(std::get<0>(args) == 100);
            REQUIRE(std::get<1>(args) == 200);
            REQUIRE(lua_gettop(L) == 2);
        }
        SECTION("Various types")
        {
            std::tuple<int, std::string, double> args{1, "no", 3.14};
            lua_pushinteger(L, 100);
            lua_pushstring(L, "yes");
            lua_pushnumber(L, 2.718);
            auto res = LuaVar::Internal::populate_arguments(L, args);
            CHECK(res);
            REQUIRE(std::get<0>(args) == 100);
            REQUIRE(std::get<1>(args) == "yes");
            REQUIRE(std::get<2>(args) == 2.718);
            REQUIRE(lua_gettop(L) == 3);
        }
        SECTION("Unsupported types - pointer")
        {
            struct SomeRandomType
            {
            } v;
            std::tuple<int, SomeRandomType *> args{1, {}};
            lua_pushinteger(L, 100);
            lua_pushlightuserdata(L, &v);
            auto res = LuaVar::Internal::populate_arguments(L, args);
            CHECK(!res);
            REQUIRE(std::get<0>(args) == 100);
            REQUIRE(std::get<1>(args) == nullptr);
            REQUIRE(lua_gettop(L) == 2);
        }
        SECTION("Unsupported types - value")
        {
            struct SomeRandomType
            {
                int v = 100;
            } obj;
            std::tuple<int, SomeRandomType> args{1, {1}};
            lua_pushinteger(L, 100);
            lua_pushlightuserdata(L, &obj);
            auto res = LuaVar::Internal::populate_arguments(L, args);
            CHECK(!res);
            REQUIRE(std::get<0>(args) == 100); // partial change is OK
            REQUIRE(std::get<1>(args).v == 1);
            REQUIRE(lua_gettop(L) == 2);
        }
        SECTION("No arguments")
        {
            std::tuple<> args{};
            auto res = LuaVar::Internal::populate_arguments(L, args);
            CHECK(res);
            REQUIRE(lua_gettop(L) == 0);
        }
    }
}

TEST_CASE("Basic func")
{
    lua_State *L = luaL_newstate();
    noValueCalled = 0;

    SECTION("Lua to C++ calling")
    {
        SECTION("func without arguments return int")
        {
            // LuaBind(L, foo0, "foo0");
            // static auto vv = LuaVar::CppFunction<decltype(foo0), foo0>("foo0");
            // vv.Bind(L);
            LuaVar::CppBind<foo0>(L, "foo0");
            luaL_dostring(L, "res = foo0()");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 1);
        }
        SECTION("func with int arg, return int")
        {
            LuaBind(L, foo1, "foo1");
            luaL_dostring(L, "res = foo1(15)");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 15);
        }
        SECTION("func with 2xint arg, return int")
        {
            LuaBind(L, foo2, "foo2");
            luaL_dostring(L, "res = foo2(3,7)");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 21);
        }
        SECTION("func with 3xint arg, return int")
        {
            LuaBind(L, xyzcalc, "xyzcalc");
            luaL_dostring(L, "res = xyzcalc(3,5,7)");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 105);
        }
        SECTION("invalid argument")
        {
            LuaBind(L, xyzcalc, "xyzcalc");
            luaL_dostring(L, "res = xyzcalc(3,\"somestring\",7)");
            lua_getglobal(L, "res");
            REQUIRE(lua_type(L, -1) == LUA_TSTRING);
            REQUIRE(std::string(luaL_checkstring(L, -1)) == "Invalid arguments");
        }
        SECTION("func with too many arguments")
        {
            // LUA allows calling functions with more arguments than needed
            LuaBind(L, foo1, "foo1");
            luaL_dostring(L, "res = foo1(100, 10, 11)");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 100);
        }
        SECTION("too few arguments - missing int")
        {
            // LUA fills in any missing arguments as nones, so the integer will default to 0
            LuaBind(L, foo1, "foo1");
            luaL_dostring(L, "res = foo1()");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 0);
        }
        SECTION("too few arguments - missing str")
        {
            // LUA fills in any missing arguments as nones, the function will return error string
            LuaBind(L, foo1str, "foo1str");
            luaL_dostring(L, "res = foo1str()");
            lua_getglobal(L, "res");
            auto str = lua_tostring(L, -1);
            REQUIRE(std::string(str) == "Invalid arguments");
        }
        SECTION("func with int arg, void return")
        {
            LuaBind(L, NoValueFoo, "NoValueFoo");
            luaL_dostring(L, "NoValueFoo(22)");
            CHECK(lua_isnil(L, -1));
        }
        SECTION("multiple func")
        {
            LuaBind(L, foo0, "foo0");
            LuaBind(L, foo1, "foo1");
            luaL_dostring(L, "res = foo1(foo0())");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 1);
        }
        SECTION("non-capture lambda")
        {
            SECTION("func with 2xint arg, int return")
            {
                auto twointslambda = [](int i, int j)
                {
                    return 22 + i + j;
                };
                LuaVar::CppBind<twointslambda>(L, "twointslambda");
                luaL_dostring(L, "res = twointslambda(22, 1)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 45);
            }
            SECTION("func with int arg, int return")
            {
                auto intlambda = [](int i)
                {
                    return 22 + i;
                };
                LuaVar::CppBind<intlambda>(L, "intlambda");
                luaL_dostring(L, "res = intlambda(22)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 44);
            }
        }
        //todo: capture lambda is not supported today
        // SECTION("capture lambda")
        // {
        //     SECTION("func with 2xint arg, int return")
        //     {
        //         int k = 10;
        //         auto captwointslambda = [&](int i, int j)
        //         {
        //             return 22+i+j+k;
        //         };
        //         LuaVar::CppBind<captwointslambda>(L, "captwointslambda");
        //         luaL_dostring(L, "res = captwointslambda(22, 1)");
        //         lua_getglobal(L, "res");
        //         REQUIRE(lua_tonumber(L, -1) == 55);
        //     }
        // }
    }
    SECTION("C++ to LUA calling")
    {
        SECTION("template deduction")
        {
            LuaVar::LuaFunction<std::tuple<int>(*)(), LuaVar::LuaFlags<
                LuaVar::LuaCallDefaultMode | LuaVar::LuaVariableValueCountReturned> >("func");
            LuaVar::LuaFunction("func", foo0);
            LuaVar::LuaFunction("func", foo0,
                                LuaVar::LuaFlags<LuaVar::LuaCallDefaultMode>{});
            // LuaVar::LuaFunction<int(*)()>("func", LuaVar::LuaFlags<LuaVar::LuaCallDefaultMode | LuaVar::LuaVariableValueCountReturned>{});
            LuaVar::LuaFunction<int(*)()>("func").Flags(
                LuaVar::LuaFlags<LuaVar::LuaCallDefaultMode>{});
            // LuaVar::LuaFunction("somefunc")
            //         .Signature<int(*)()>()
            //         .Flags<LuaVar::LuaFlags<LuaVar::LuaCallDefaultMode | LuaVar::LuaVariableValueCountReturned> >();
        }
        SECTION("func without arguments - return int")
        {
            auto func = LuaVar::LuaFunction<int(*)()>("func");
            luaL_dostring(L, "function func() return 5; end");
            auto res = func(L);
            REQUIRE(res == 5);
        }
        SECTION("func without arguments - return single int - multi return value mode")
        {
            auto func = LuaVar::LuaFunction<std::tuple<int>(*)(), LuaVar::LuaFlags<
                LuaVar::LuaCallDefaultMode | LuaVar::LuaVariableValueCountReturned> >("func");
            luaL_dostring(L, "function func() return 5; end");
            auto res = func(L);
            REQUIRE(std::get<0>(res) == 5);
        }
        SECTION("func without arguments - return multiple ints - multi return value mode")
        {
            auto func = LuaVar::LuaFunction<std::tuple<int,int,int>(*)(), LuaVar::LuaFlags<
                LuaVar::LuaCallDefaultMode | LuaVar::LuaVariableValueCountReturned> >("func");
            luaL_dostring(L, "function func() return 5,10,15; end");
            auto res = func(L);
            REQUIRE(std::get<0>(res) == 5);
            REQUIRE(std::get<1>(res) == 10);
            REQUIRE(std::get<2>(res) == 15);
        }
        SECTION("func without arguments - return not enough ints - multi return value mode")
        {
            auto func = LuaVar::LuaFunction<std::tuple<int,int,int,int,int>(*)(), LuaVar::LuaFlags<
                LuaVar::LuaCallDefaultMode | LuaVar::LuaVariableValueCountReturned> >("func");
            luaL_dostring(L, "function func() return 5, 10; end");
            auto res = func(L);
            REQUIRE(std::get<0>(res) == 5);
            REQUIRE(std::get<1>(res) == 10);
            REQUIRE(std::get<2>(res) == 15);
        }
        SECTION("func without arguments - return too many ints - multi return value mode")
        {
            auto func = LuaVar::LuaFunction<std::tuple<int,int>(*)(), LuaVar::LuaFlags<
                LuaVar::LuaCallDefaultMode | LuaVar::LuaVariableValueCountReturned> >("func");
            luaL_dostring(L, "function func() return 5, 10, 15; end");
            auto res = func(L);
            REQUIRE(std::get<0>(res) == 5);
            REQUIRE(std::get<1>(res) == 10);
        }
        SECTION("func without arguments - return too many ints - without multi return value mode")
        {
            auto func = LuaVar::LuaFunction<int(*)(), LuaVar::LuaFlags<LuaVar::LuaCallDefaultMode> >("func");
            luaL_dostring(L, "function func() return 5, 10, 15; end");
            auto res = func(L);
            REQUIRE(res == 5);
        }
        SECTION("func without arguments - return ints - without variable return count value mode")
        {
            auto func = LuaVar::LuaFunction<std::tuple<int,int,int>(*)(), LuaVar::LuaFlags<LuaVar::LuaCallDefaultMode> >("func");
            luaL_dostring(L, "function func() return 5, 10, 15; end");
            auto res = func(L);
            REQUIRE(std::get<0>(res) == 5);
            REQUIRE(std::get<1>(res) == 10);
            REQUIRE(std::get<2>(res) == 15);
        }
        //todo: create test cases that test erroring out if soft errors are not enabled - if returned value count doesn't match expected - fail
    }
}

//todo: move to other test file
TEST_CASE("Look for mem leaks")
{
    lua_State *L = luaL_newstate();
    SECTION("invalid argument")
    {
        // CrtCheckMemory check;
        LuaBind(L, xyzcalc, "xyzcalc");
        int i = 0;
        while (i < 1000000)
        {
            luaL_dostring(L, "res = xyzcalc(3,\"somestring\",7)");
            ++i;
        }
        lua_getglobal(L, "res");
        REQUIRE(lua_type(L, -1) == LUA_TSTRING);
    }
}
