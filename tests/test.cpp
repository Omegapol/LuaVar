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
    SECTION("Test pushing arguments")
    {
        SECTION("integer")
        {
            int i = 100;
            LuaVar::Internal::push_result(L, i);
            CHECK(lua_tonumber(L, -1) == 100);
        }
        SECTION("double")
        {
            double j = 200.0;
            LuaVar::Internal::push_result(L, j);
            CHECK(lua_tonumber(L, -1) == 200.0);
        }
        SECTION("boolean")
        {
            bool k = true;
            LuaVar::Internal::push_result(L, k);
            CHECK(lua_toboolean(L, -1));
        }
        SECTION("string")
        {
            const char *ptr = "yes";
            LuaVar::Internal::push_result(L, ptr);
            CHECK(std::string(lua_tostring(L, -1)) == "yes");
        }
        SECTION("cstring")
        {
            std::string str = "no";
            LuaVar::Internal::push_result(L, str);
            CHECK(std::string(lua_tostring(L, -1)) == "no");
        }
        SECTION("tuple - multiple results") {
            auto tup = std::tuple{"yes", 123, 123.0, true};
            LuaVar::Internal::push_result(L, tup);
            auto res = lua_tostring(L, -4);
            REQUIRE(res != nullptr);
            CHECK(std::string(res) == "yes");
            CHECK(lua_tonumber(L, -3) == 123);
            CHECK(lua_tonumber(L, -2) == 123.0);
            CHECK(lua_toboolean(L, -1));
        }
    }
}

template<typename T>
struct IsValidCppFunction
{
    static constexpr bool value = requires { LuaVar::CppFunction<T>; };
};

template<auto T>
struct IsValidCppFunctionNT
{
    static constexpr bool value = requires { LuaVar::CppFunction<T>; };
};

template<auto T>
struct IsValidCallable
{
    static constexpr bool value = requires { typename LuaVar::Callable<T>; };
};

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
            LuaVar::CppFunction<foo0>("foo0").Bind(L);
            luaL_dostring(L, "res = foo0()");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 1);
        }
        SECTION("bind with function statically")
        {
            LuaVar::CppFunction<foo0>("foo0").Bind(L);
            luaL_dostring(L, "res = foo0()");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 1);
        }
        SECTION("bind with function statically - also provide function as arg")
        {
            LuaVar::CppFunction<foo0>("foo0", foo0).Bind(L);
            luaL_dostring(L, "res = foo0()");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 1);
        }
        SECTION("bind with function as argument dynamically")
        {
            LuaVar::CppFunction("foo0", LuaVar::CallableDyn{foo0}).Bind(L);
            luaL_dostring(L, "res = foo0()");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 1);
        }
        SECTION("bind with function as argument dynamically using flags")
        {
            LuaVar::CppFunction("foo0", LuaVar::CallableDyn{foo0},
                                 LuaVar::LuaFlags<LuaVar::LuaCallDefaultMode>{}).Bind(L);
            luaL_dostring(L, "res = foo0()");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 1);
        }
        SECTION("bind with function as argument statically")
        {
            LuaVar::CppFunction("foo0", LuaVar::Callable<foo0>{}).Bind(L);
            luaL_dostring(L, "res = foo0()");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 1);
        }
        SECTION("fail on invalid argument")
        {
            STATIC_CHECK_FALSE(IsValidCppFunctionNT<1234>::value);
            STATIC_CHECK_FALSE(IsValidCallable<1234>::value);
            // STATIC_CHECK(IsValidCppFunctionNT<foo0>::value); //todo: create type check for CppFunction
            STATIC_CHECK(IsValidCallable<foo0>::value);
        }
        SECTION("func with int arg, return int")
        {
            LuaVar::CppFunction<foo1>("foo1").Bind(L);
            luaL_dostring(L, "res = foo1(15)");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 15);
        }
        SECTION("func with 2xint arg, return int")
        {
            LuaVar::CppFunction<foo2>("foo2").Bind(L);
            luaL_dostring(L, "res = foo2(3,7)");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 21);
        }
        SECTION("func with 3xint arg, return int")
        {
            LuaVar::CppFunction<xyzcalc>("xyzcalc").Bind(L);
            luaL_dostring(L, "res = xyzcalc(3,5,7)");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 105);
        }
        SECTION("invalid argument")
        {
            LuaVar::CppFunction<xyzcalc>("xyzcalc").Bind(L);
            luaL_dostring(L, "res = xyzcalc(3,\"somestring\",7)");
            lua_getglobal(L, "res");
            REQUIRE(lua_type(L, -1) == LUA_TSTRING);
            REQUIRE(std::string(luaL_checkstring(L, -1)) == "Invalid arguments");
        }
        SECTION("func with too many arguments")
        {
            // LUA allows calling functions with more arguments than needed
            LuaVar::CppFunction<foo1>("foo1").Bind(L);
            luaL_dostring(L, "res = foo1(100, 10, 11)");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 100);
        }
        SECTION("too few arguments - missing int")
        {
            // LUA fills in any missing arguments as nones, so the integer will default to 0
            LuaVar::CppFunction<foo1>("foo1").Bind(L);
            luaL_dostring(L, "res = foo1()");
            lua_getglobal(L, "res");
            REQUIRE(lua_tonumber(L, -1) == 0);
        }
        SECTION("too few arguments - missing str")
        {
            // LUA fills in any missing arguments as nones, the function will return error string
            LuaVar::CppFunction<foo1str>("foo1str").Bind(L);
            luaL_dostring(L, "res = foo1str()");
            lua_getglobal(L, "res");
            auto str = lua_tostring(L, -1);
            REQUIRE(std::string(str) == "Invalid arguments");
        }
        SECTION("func with int arg, void return")
        {
            LuaVar::CppFunction<NoValueFoo>("NoValueFoo").Bind(L);
            luaL_dostring(L, "NoValueFoo(22)");
            CHECK(lua_isnil(L, -1));
        }
        SECTION("multiple func")
        {
            LuaVar::CppFunction<foo0>("foo0").Bind(L);
            LuaVar::CppFunction<foo1>("foo1").Bind(L);
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
                LuaVar::CppFunction<twointslambda>("twointslambda").Bind(L);
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
                LuaVar::CppFunction<intlambda>("intlambda").Bind(L);
                luaL_dostring(L, "res = intlambda(22)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 44);
            }
            SECTION("binding with lambda as argument")
            {
                auto intlambda = [](int i)
                {
                    return 22 + i;
                };
                LuaVar::CppFunction("intlambda", intlambda).Bind(L);
                STATIC_CHECK(sizeof(intlambda) == 1);
                luaL_dostring(L, "res = intlambda(22)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 44);
            }
        }
        SECTION("capture by ref lambda")
        {
            SECTION("func with 2xint arg, int return")
            {
                int k = 16;
                auto captwointslambda = [&](int i, int j)
                {
                    return 22 + i + j + k;
                };
                LuaVar::CppFunction("captwointslambda", captwointslambda).Bind(L);
                CHECK(captwointslambda(22,1) == 61);
                luaL_dostring(L, "res = captwointslambda(22, 1)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 61);
                lua_pop(L, -1);

                k = 32;

                luaL_dostring(L, "res = captwointslambda(22, 1)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 77);
                lua_pop(L, -1);

                lua_gc(L, LUA_GCCOLLECT, 0);
                luaL_dostring(L, "captwointslambda = nil");
                lua_gc(L, LUA_GCCOLLECT, 0);
            }
            SECTION("func with 2xint arg int return CallableDyn")
            {
                int k = 16;
                auto captwointslambda = [&](int i, int j)
                {
                    return 22 + i + j + k;
                };
                LuaVar::CppFunction("captwointslambda", LuaVar::CallableDyn{captwointslambda}).Bind(L);
                luaL_dostring(L, "res = captwointslambda(22, 1)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 61);
                lua_pop(L, -1);

                k = 32;

                luaL_dostring(L, "res = captwointslambda(22, 1)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 77);
                lua_pop(L, -1);

                lua_gc(L, LUA_GCCOLLECT, 0);
                luaL_dostring(L, "captwointslambda = nil");
                lua_gc(L, LUA_GCCOLLECT, 0);
            }
            SECTION("by rvalue")
            {
                int k = 16;
                LuaVar::CppFunction("captwointslambda", [&](int i, int j)
                {
                    return 22 + i + j + k;
                }).Bind(L);
                luaL_dostring(L, "res = captwointslambda(22, 1)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 61);
                lua_pop(L, -1);
            }
        }
        SECTION("capture by value lambda")
        {
            struct ExampleStruct
            {
                static int &DestructionCount()
                {
                    static int DestructionCount = 0;
                    return DestructionCount;
                }

                ~ExampleStruct()
                {
                    ++DestructionCount();
                }

                std::string v = "yes";
                int val = 100;
            } some_struct;
            ExampleStruct::DestructionCount() = 0;
            SECTION("func with 2xint arg, int return")
            {
                int k = 10;
                auto captwointslambda = [=](int i, int j)
                {
                    return 22 + i + j + k;
                };
                // LuaVar::CppBind(L, "captwointslambda", captwointslambda);
                LuaVar::CppFunction("captwointslambda", captwointslambda).Bind(L);
                luaL_dostring(L, "res = captwointslambda(22, 1)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 55);
                lua_gc(L, LUA_GCCOLLECT, 0);
                luaL_dostring(L, "captwointslambda = nil");
                lua_gc(L, LUA_GCCOLLECT, 0);
            }
            // check that we can use multiple lambdas at once,
            // check if they are destroyed properly
            // overwriting variable with new lambda should work as well
            SECTION("multiple lambdas - different")
            {
                int k = 16;
                LuaVar::CppFunction("captwointslambda", [=](int i, int j)
                {
                    return 22 + i + j + k;
                }).Bind(L);
                LuaVar::CppFunction("captwointslambda2", [=](int i, int j)
                {
                    return static_cast<int>(23 + some_struct.v.length() + some_struct.val + i + j);
                }).Bind(L);
                luaL_dostring(L, "res = captwointslambda(22, 1)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 61);
                lua_pop(L, -1);
                luaL_dostring(L, "res = captwointslambda2(22, 1)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 149);
                lua_pop(L, -1);

                // struct destructor should be called given times because we are copying lambda around
                auto destructions_before_gc = ExampleStruct::DestructionCount();
                CHECK(ExampleStruct::DestructionCount() == 2);

                // clear lua variables and force garbage collection
                luaL_dostring(L, "captwointslambda = nil");
                luaL_dostring(L, "captwointslambda2 = nil");
                lua_gc(L, LUA_GCCOLLECT, 0);
                // the destructor should have been called by now, it should be called exactly once
                CHECK((ExampleStruct::DestructionCount()-destructions_before_gc) == 1);

                // bind the brand new lambda again and test it
                LuaVar::CppFunction("captwointslambda2", [=](int i, int j)
                {
                    return static_cast<int>(23 + some_struct.v.length() + some_struct.val + i + j);
                }).Bind(L);
                luaL_dostring(L, "res = captwointslambda2(23, 2)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 151);
                lua_pop(L, -1);
                // struct destructor should be called given times because we are copying lambda around
                destructions_before_gc = ExampleStruct::DestructionCount();
                CHECK(ExampleStruct::DestructionCount() == 5);

                // clear lua variable and force garbage collection
                luaL_dostring(L, "captwointslambda2 = nil");
                lua_gc(L, LUA_GCCOLLECT, 0);
                // the destructor should have been called by now, it should be called exactly once
                CHECK((ExampleStruct::DestructionCount()-destructions_before_gc) == 1);
            }
            // check that same lambda instance can be bound multiple times
            SECTION("multiple lambdas - reusing same lambda")
            {
                int k = 16;
                LuaVar::CppFunction("captwointslambda", [=](int i, int j)
                {
                    return 22 + i + j + k;
                }).Bind(L);
                auto reused_lambda = [=](int i, int j)
                {
                    return static_cast<int>(23 + some_struct.v.length() + some_struct.val + i + j);
                };
                LuaVar::CppFunction("captwointslambda2", reused_lambda).Bind(L);
                luaL_dostring(L, "res = captwointslambda(22, 1)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 61);
                lua_pop(L, -1);
                luaL_dostring(L, "res = captwointslambda2(22, 1)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 149);
                lua_pop(L, -1);\

                // struct destructor should be called given times because we are copying lambda around
                auto destructions_before_gc = ExampleStruct::DestructionCount();
                CHECK(ExampleStruct::DestructionCount() == 1);

                // clear lua variables and force garbage collection
                luaL_dostring(L, "captwointslambda = nil");
                luaL_dostring(L, "captwointslambda2 = nil");
                lua_gc(L, LUA_GCCOLLECT, 0);
                // the destructor should have been called by now, it should be called exactly once
                CHECK((ExampleStruct::DestructionCount()-destructions_before_gc) == 1);

                // bind the same lambda again and test it
                LuaVar::CppFunction("captwointslambda2", reused_lambda).Bind(L);
                luaL_dostring(L, "res = captwointslambda2(23, 2)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 151);
                lua_pop(L, -1);

                // struct destructor should be called given times because we are copying lambda around
                destructions_before_gc = ExampleStruct::DestructionCount();
                CHECK(ExampleStruct::DestructionCount() == 3);

                // clear lua variable and force garbage collection
                luaL_dostring(L, "captwointslambda2 = nil");
                lua_gc(L, LUA_GCCOLLECT, 0);
                // the destructor should have been called by now, it should be called exactly once
                CHECK((ExampleStruct::DestructionCount()-destructions_before_gc) == 1);
            }
            SECTION("already bound lambda should still work after the source lambda goes out of scope")
            {
                {
                    auto reused_lambda = [=](int i, int j)
                    {
                        return static_cast<int>(23 + some_struct.v.length() + some_struct.val + i + j);
                    };
                    LuaVar::CppFunction("captwointslambda2", reused_lambda).Bind(L);
                }
                luaL_dostring(L, "res = captwointslambda2(22, 1)");
                lua_getglobal(L, "res");
                REQUIRE(lua_tonumber(L, -1) == 149);
                lua_pop(L, -1);

                auto destructions_before_gc = ExampleStruct::DestructionCount();
                CHECK(ExampleStruct::DestructionCount() == 2);

                luaL_dostring(L, "captwointslambda2 = nil");
                lua_gc(L, LUA_GCCOLLECT, 0);
                // the destructor should have been called by now, it should be called exactly once
                CHECK((ExampleStruct::DestructionCount()-destructions_before_gc) == 1);
            }
        }
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
        SECTION("func without arguments - void return")
        {
            auto func = LuaVar::LuaFunction<void(*)()>("func");
            luaL_dostring(L, "function func() return 5; end");
            func(L);
            STATIC_REQUIRE(std::is_same_v<decltype(func(L)), void>);
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
            auto func = LuaVar::LuaFunction<std::tuple<int, int, int>(*)(), LuaVar::LuaFlags<
                LuaVar::LuaCallDefaultMode | LuaVar::LuaVariableValueCountReturned> >("func");
            luaL_dostring(L, "function func() return 5,10,15; end");
            auto res = func(L);
            REQUIRE(std::get<0>(res) == 5);
            REQUIRE(std::get<1>(res) == 10);
            REQUIRE(std::get<2>(res) == 15);
        }
        SECTION("func without arguments - return not enough ints - multi return value mode")
        {
            // if lua returns fewer results than expected, other expected results has default values
            auto func = LuaVar::LuaFunction<std::tuple<int, int, int, int, int>(*)(), LuaVar::LuaFlags<
                LuaVar::LuaCallDefaultMode | LuaVar::LuaVariableValueCountReturned> >("func");
            luaL_dostring(L, "function func() return 5, 10; end");
            auto res = func(L);
            REQUIRE(std::get<0>(res) == 5);
            REQUIRE(std::get<1>(res) == 10);
            REQUIRE(std::get<2>(res) == 0);
        }
        SECTION("func without arguments - return too many ints - multi return value mode")
        {
            auto func = LuaVar::LuaFunction<std::tuple<int, int>(*)(), LuaVar::LuaFlags<
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
            auto func = LuaVar::LuaFunction<std::tuple<int, int, int>(*)(), LuaVar::LuaFlags<
                LuaVar::LuaCallDefaultMode> >("func");
            luaL_dostring(L, "function func() return 5, 10, 15; end");
            auto res = func(L);
            REQUIRE(std::get<0>(res) == 5);
            REQUIRE(std::get<1>(res) == 10);
            REQUIRE(std::get<2>(res) == 15);
        }
        //todo: create test cases that test erroring out if soft errors are not enabled - if returned value count doesn't match expected - fail
    }
}

TEST_CASE("Assumptions")
{
    int k = 16;
    auto captwointslambda = [&](int i, int j)
    {
        return 22 + i + j + k;
    };
    char ll2 = 'a';
    auto captwointslambda2 = [=](int /*i*/, int /*j*/)
    {
        return 22 + static_cast<int>(ll2);
    };
    auto captwointslambda3 = [](int /*i*/, int /*j*/)
    {
        return 22;
    };
    SECTION("Capture lambda detection")
    {
        // STATIC_CHECK(std::is_pod_v<decltype(captwointslambda)>);
        STATIC_CHECK(
            !(std::is_standard_layout_v<decltype(captwointslambda)> &&
                std::is_trivial_v<decltype(captwointslambda)>));


        // STATIC_CHECK(std::is_pod_v<decltype(captwointslambda2)>);
        STATIC_CHECK(
            !(std::is_standard_layout_v<decltype(captwointslambda2)> &&
                std::is_trivial_v<decltype(captwointslambda2)>));

        // STATIC_CHECK(std::is_pod_v<decltype(captwointslambda3)>);
        STATIC_CHECK(
            std::is_standard_layout_v<decltype(captwointslambda3)> &&
            std::is_trivial_v<decltype(captwointslambda3)>);
    }
}

//todo: move to other test file
TEST_CASE("Look for mem leaks")
{
    lua_State *L = luaL_newstate();
    SECTION("invalid argument")
    {
        // CrtCheckMemory check;
        LuaVar::CppFunction<xyzcalc>("xyzcalc").Bind(L);
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
