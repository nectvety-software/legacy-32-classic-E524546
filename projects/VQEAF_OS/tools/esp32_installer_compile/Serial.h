#pragma once
struct FakeSerial {
 template<class... T> void printf(const char*,T...){}
};
static FakeSerial Serial;
