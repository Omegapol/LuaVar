#include <iostream>

#include <lua.hpp>
#include <luavar/luavar.h>
#include <cmath>


double foo(double arg, double arg2) {
    printf("foo\n");
    fflush(stdout);
    return sin(arg+ arg2);
}
double foob(double arg, double arg2, std::string s) {
    printf("%s", s.c_str());
    fflush(stdout);
    return sin(arg+ arg2);
}

LuaDeclareV(test2_invalid, bool(*)(void), LuaVar::LuaCallSoftError);
//LuaDeclare(test2, bool(*)(void));
LuaDeclare(test2, bool(*)(int));

//#define ENCODE0(x) x arg0
//#define ENCODE1(x,...) x arg1, ENCODE0(__VA_ARGS__)
//#define ENCODE2(x,...) x arg2, ENCODE1(__VA_ARGS__)
//#define ENCODE3(x,...) x arg3, ENCODE2(__VA_ARGS__)
//#define ENCODE4(x,...) x arg4, ENCODE3(__VA_ARGS__)
//#define ENCODE5(x,...) x arg5, ENCODE4(__VA_ARGS__)
//#define ENCODE6(x,...) AnsiString(x), ENCODE5(__VA_ARGS__)
////Add more pairs if required. 6 is the upper limit in this case.
//#define ENCODE(i,...) ENCODE##i(__VA_ARGS__) //i is the number of arguments (max 6 in this case)
//
//#define LOGG(RetType, name, count,...) RetType name(ENCODE(count,__VA_ARGS__))

LuaDeclareB(bool, test5, int);
LuaDeclareC(bool, test6, int);

bool test7(int, int);

//LOGG(bool, test4, 1, int, double);

auto test123 = LuaVar::LuaFunction<bool(*)(int)>("test6");
auto test1234 = LuaVar::LuaFunction("test5", test7);

int main() {
    // int i = 5;
//    call(i);

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    LuaBind(L, foo, "test");
    LuaBind(L, foob, "calc");

    std::cout << "Hello, World!" << std::endl;

    auto res = luaL_dofile(L, "main.lua");
    const char* err = nullptr;
    if(res != LUA_OK) {
        err = lua_tostring(L, -1);
        printf("%s", err);
    }
    assert(err == nullptr);

    test123.Call(L, 1);
    test123(L, 5);
    test1234(L, 11, 10);
    // return 0;

    /*auto res2 = */LuaCall(L, double(*)(double, double), "testFunc", 3.14, 1.57);

    std::tuple<int, double> t{1, 2.0};

    test2_invalid(L, {});
    /*auto res3 = */LuaCall(L, bool(*)(int), "testFunc", 11);
    test2(L, {2});

//    test5(L, 5);
    test5(L, 5);

    LuaCall(L, double(*)(), "test6");
    test6(L, {1});



    return 0;
}
