#include "LuaCoreAPI.h"

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "LuaSys.h"
//#include "LuaConsole.h"

#include "Misc/EmuBase.h"
#include "Misc/GlobalConfig.h"
#include "Misc/GameConfig.h"
#include "Util/GraphicsView.h"
#include <ImGuiSupport/ImGuiScaling.h>



static int print(lua_State* pState)
{
    int nargs = lua_gettop(pState);

    for (int i=1; i <= nargs; i++) 
    {
        if (lua_isstring(pState, i))
        {
            /* Pop the next arg using lua_tostring(L, i) and do your print */
            const char* pString = lua_tostring(pState, i);
            LuaSys::OutputDebugString("%s", pString);
        }
        else 
        {
        /* Do something with non-strings if you like */
        }
    }

    return 0;
}

static int ReadByte(lua_State* pState)
{
    FEmuBase* pEmu = LuaSys::GetEmulator();
    
    if(pEmu != nullptr && lua_isinteger(pState, -1))
    {
        const lua_Integer address = lua_tointeger(pState, -1);
        lua_pushinteger(pState, pEmu->ReadByte((uint16_t)address));
        return 1;
    }
    
    return 0;
}

static int ReadWord(lua_State* pState)
{
    FEmuBase* pEmu = LuaSys::GetEmulator();
    
    if(pEmu != nullptr && lua_isinteger(pState, -1))
    {
        const lua_Integer address = lua_tointeger(pState, -1);
        lua_pushinteger(pState, pEmu->ReadWord((uint16_t)address));
        return 1;
    }
    
    return 0;
}

static int GetMemPtr(lua_State* pState)
{
    FEmuBase* pEmu = LuaSys::GetEmulator();
    
    if(pEmu != nullptr && lua_isinteger(pState, -1))
    {
        const lua_Integer address = lua_tointeger(pState, -1);
        lua_pushlightuserdata(pState, (void*)pEmu->GetMemPtr((uint16_t)address));
        return 1;
    }
    
    return 0;
}

static int GetImageScale(lua_State* pState)
{
	lua_pushnumber(pState, ImGui_GetScaling());
	return 1;
}


static int ClearGraphicsView(lua_State *pState)
{
    FGraphicsView* pGraphicsView = (FGraphicsView*)lua_touserdata(pState, 1 );
    if(pGraphicsView == nullptr)
        return 0;

    uint32_t clearCol = 0;
    if(lua_isinteger(pState, 2))
        clearCol = (uint32_t)lua_tointeger(pState, 2);
    pGraphicsView->Clear(clearCol);
    return 0;
}

static int DrawGraphicsView(lua_State *pState)
{
    FGraphicsView* pGraphicsView = (FGraphicsView*)lua_touserdata(pState, 1 );
    if (pGraphicsView == nullptr)
        return 0;

    pGraphicsView->UpdateTexture();
    pGraphicsView->Draw();
    return 0;
}

static int SaveGraphicsViewPNG(lua_State *pState)
{
    FGraphicsView* pGraphicsView = (FGraphicsView*)lua_touserdata(pState, 1 );
    if (pGraphicsView == nullptr)
        return 0;
    
    FEmuBase* pEmulator = LuaSys::GetEmulator();
    const std::string gameRoot = pEmulator->GetGlobalConfig()->WorkspaceRoot + pEmulator->GetGameConfig()->Name + "/";
    const std::string fname = gameRoot + luaL_optstring(pState, 2, "temp.png");
    
    pGraphicsView->SavePNG(fname.c_str());
    return 0;
}

static int DrawOtherGraphicsViewScaled(lua_State *pState)
{
    FGraphicsView* pGraphicsView = (FGraphicsView*)lua_touserdata(pState, 1 );
    if (pGraphicsView == nullptr)
        return 0;

    FGraphicsView* pOtherGraphicsView = (FGraphicsView*)lua_touserdata(pState, 1 );
    if (pOtherGraphicsView == nullptr)
        return 0;

    const int xp = (int)luaL_optinteger(pState,3, 0);
    const int yp = (int)luaL_optinteger(pState,4, 0);
    const int xsize = (int)luaL_optinteger(pState,5, 64);
    const int ysize = (int)luaL_optinteger(pState,6, 64);

    pGraphicsView->DrawOtherGraphicsViewScaled(pOtherGraphicsView, xp, yp, xsize, ysize);
}

static const luaL_Reg corelib[] =
{
    {"print", print},
    {"ReadByte", ReadByte},
    {"ReadWord", ReadWord},
    {"GetMemPtr", GetMemPtr},
	{"GetImageScale", GetImageScale},
    {"ClearGraphicsView", ClearGraphicsView},
    {"DrawGraphicsView", DrawGraphicsView},
    {"SaveGraphicsViewPNG", SaveGraphicsViewPNG},
    {"DrawOtherGraphicsViewScaled", DrawOtherGraphicsViewScaled},

    {NULL, NULL}    // terminator
};


int luaopen_corelib(lua_State *pState)
{
    lua_getglobal(pState, "_G");
    luaL_setfuncs(pState, corelib, 0);  // for Lua versions 5.2 or greater
    lua_pop(pState, 1);
    return 1;
}

