#pragma once
#include <iostream>
#include <vector>
#include <span>
#include <string>
#include <string_view>

extern "C" {
    #include "pl/lua.h"
    #include "pl/lauxlib.h"
    #include "pl/lualib.h"
}

#include <pl/Mod.hpp>          //[span_4](start_span)[span_4](end_span)
#include "pl/memory/Signature.hpp"
#include "pl/memory/Hook.hpp"
#include "pl/memory/Patch.hpp"

namespace pl::lua_binding {

// Registry references untuk menyimpan callback lifecycle dari Lua
inline int luaEnableRef = LUA_REFNIL;
inline int luaDisableRef = LUA_REFNIL;

// ==========================================
// 1. BINDING FOR: pl/Mod.hpp (Metadata & Log)
// ==========================================
static int lua_mod_registerLifecycle(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    // Lua hanya perlu mendaftarkan event runtime (enable dan disable)
    lua_getfield(L, 1, "enable");
    if (lua_isfunction(L, -1)) {
        luaEnableRef = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
        lua_pop(L, 1);
    }

    lua_getfield(L, 1, "disable");
    if (lua_isfunction(L, -1)) {
        luaDisableRef = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
        lua_pop(L, 1);
    }

    return 0;
}

static int lua_mod_current(lua_State* L) {
    auto* currentMod = pl::mod::NativeMod::current();
    if (!currentMod) {
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);
    lua_pushstring(L, currentMod->getId().c_str());      lua_setfield(L, -2, "id");
    lua_pushstring(L, currentMod->getName().c_str());    lua_setfield(L, -2, "name");
    lua_pushstring(L, currentMod->getAuthor().c_str());  lua_setfield(L, -2, "author");
    lua_pushstring(L, currentMod->getVersion().c_str()); lua_setfield(L, -2, "version");
    
    lua_pushstring(L, currentMod->getModDir().string().c_str());       lua_setfield(L, -2, "modDir");
    lua_pushstring(L, currentMod->getDataDir().string().c_str());      lua_setfield(L, -2, "dataDir");
    lua_pushstring(L, currentMod->getConfigDir().string().c_str());    lua_setfield(L, -2, "configDir");
    lua_pushstring(L, currentMod->getResourceDir().string().c_str());   lua_setfield(L, -2, "resourceDir");
    
    return 1;
}

static int lua_mod_log_info(lua_State* L) {
    auto* currentMod = pl::mod::NativeMod::current();
    if (currentMod) {
        currentMod->getLogger().info(luaL_checkstring(L, 1));
    }
    return 0;
}

static int lua_mod_log_error(lua_State* L) {
    auto* currentMod = pl::mod::NativeMod::current();
    if (currentMod) {
        currentMod->getLogger().error(luaL_checkstring(L, 1));
    }
    return 0;
}

static const struct luaL_Reg mod_funcs[] = {
    {"registerLifecycle", lua_mod_registerLifecycle},
    {"current", lua_mod_current},
    {"info", lua_mod_log_info},
    {"error", lua_mod_log_error},
    {NULL, NULL}
};

// ==========================================
// 2. BINDING FOR: pl/memory/Signature.hpp
// ==========================================
static int lua_mem_resolveSignature(lua_State* L) {
    std::string_view signature = luaL_checkstring(L, 1);
    std::string_view moduleName = luaL_checkstring(L, 2);
    uintptr_t address = pl::memory::resolveSignature(signature, moduleName);
    lua_pushinteger(L, (lua_Integer)address);
    return 1;
}

// ==========================================
// 3. BINDING FOR: pl/memory/Patch.hpp
// ==========================================
static int lua_mem_writeBytes(lua_State* L) {
    uintptr_t address = (uintptr_t)luaL_checkinteger(L, 1);
    std::string_view name = luaL_checkstring(L, 3);
    bool success = false;

    if (lua_isstring(L, 2)) {
        std::string_view hexBytes = lua_tostring(L, 2);
        success = pl::memory::writeBytes(address, hexBytes, name);
    } else if (lua_istable(L, 2)) {
        std::vector<uint8_t> bytes;
        int index = 1;
        while (true) {
            lua_rawgeti(L, 2, index);
            if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
            bytes.push_back((uint8_t)luaL_checkinteger(L, -1));
            lua_pop(L, 1);
            index++;
        }
        success = pl::memory::writeBytes(address, std::span<const uint8_t>(bytes), name);
    }
    lua_pushboolean(L, success);
    return 1;
}

static int lua_mem_readBytes(lua_State* L) {
    uintptr_t address = (uintptr_t)luaL_checkinteger(L, 1);
    size_t length = (size_t)luaL_checkinteger(L, 2);
    std::vector<uint8_t> result = pl::memory::readBytes(address, length);
    
    lua_newtable(L);
    for (size_t i = 0; i < result.size(); ++i) {
        lua_pushinteger(L, result[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int lua_mem_revertPatch(lua_State* L) {
    lua_pushboolean(L, pl::memory::revertPatch(luaL_checkstring(L, 1)));
    return 1;
}

static int lua_mem_revertAllPatches(lua_State* L) {
    pl::memory::revertAllPatches();
    return 0;
}

// ==========================================
// 4. BINDING FOR: pl/memory/Hook.hpp
// ==========================================
static int lua_mem_hook(lua_State* L) {
    void* target = (void*)luaL_checkinteger(L, 1);
    void* detour = (void*)luaL_checkinteger(L, 2);
    int priority = luaL_optinteger(L, 3, 200);
    void* originalFunc = nullptr;
    
    int status = pl::memory::hook(target, detour, &originalFunc, static_cast<pl::memory::HookPriority>(priority));
    lua_pushinteger(L, status);
    lua_pushinteger(L, (lua_Integer)originalFunc);
    return 2;
}

static int lua_mem_unhook(lua_State* L) {
    void* target = (void*)luaL_checkinteger(L, 1);
    void* detour = (void*)luaL_checkinteger(L, 2);
    lua_pushboolean(L, pl::memory::unhook(target, detour));
    return 1;
}

static const struct luaL_Reg memory_funcs[] = {
    {"resolveSignature", lua_mem_resolveSignature},
    {"writeBytes", lua_mem_writeBytes},
    {"readBytes", lua_mem_readBytes},
    {"revertPatch", lua_mem_revertPatch},
    {"revertAllPatches", lua_mem_revertAllPatches},
    {"hook", lua_mem_hook},
    {"unhook", lua_mem_unhook},
    {NULL, NULL}
};

// ==========================================
// MASTER REGISTRATION INTERFACE
// ==========================================
inline void register_all_pl_modules(lua_State* L) {
    lua_newtable(L); 

    lua_newtable(L);
    luaL_setfuncs(L, mod_funcs, 0);
    lua_setfield(L, -2, "mod");

    lua_newtable(L);
    luaL_setfuncs(L, memory_funcs, 0);
    lua_setfield(L, -2, "memory");

    lua_setglobal(L, "pl");
}

inline bool triggerLuaLifecycle(lua_State* L, int registryRef) {
    if (registryRef == LUA_REFNIL) return true; 
    
    lua_rawgeti(L, LUA_REGISTRYINDEX, registryRef);
    if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
        std::cerr << "[Lua Lifecycle Error] " << lua_tostring(L, -1) << "\n";
        lua_pop(L, 1);
        return false;
    }
    
    bool result = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return result;
}

} // namespace pl::lua_binding
