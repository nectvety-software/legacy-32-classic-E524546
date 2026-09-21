"""Chay smoke test Lua tren PC qua lupa (LuaJIT 5.1)."""
from lupa import LuaRuntime

lua = LuaRuntime(unpack_returned_tuples=True)
lua.execute(open('tools/lua_smoke.lua', encoding='utf-8').read())
