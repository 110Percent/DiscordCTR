/*
    DiscordCTR - Discord client written in C++ for the Nintendo 3DS
    Copyright (C) 2017 Sono (https://github.com/MarcuzD)
    All rights reserved.
    Source code was only provided for education purposes.
    Please see README.md in the project's root directory for more info about licensing.
*/

#include <3ds.h>

extern "C" int __stacksize__ = 0x18000;

extern "C"
{
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/iosupport.h>

#include <poll.h>
#include <arpa/inet.h>

#include <sf2d/sf2d.h>

#include "grafx/drawer.h"
#include "grafx/font.h"
}

#include <string>
#include <iostream>
#include <ostream>
#include <fstream>
#include <iterator>

#include "misc/util.hpp"
#include "misc/ini.hpp"
#include "SimpleJSON/JSONValue.hpp"
#include "WS/WSHelper.hpp"
#include "D/DiscordWSA.hpp"
#include "D/DiscordAPI.hpp"

#include "misc/AsyncTextInput.hpp"


using std::string;
using std::advance;
using std::to_string;
using std::fstream;
using std::cout;
using std::endl;
using MM::Ini;
using namespace MM::WS;
using namespace MM::D;
using namespace MM::MISC;



#define hangmacro() \
({\
    puts("Press a key to exit...");\
    while(aptMainLoop())\
    {\
        hidScanInput();\
        if(hidKeysDown())\
        {\
            goto killswitch;\
        }\
        gspWaitForVBlank();\
    }\
})


static jmp_buf __exc;
static int  __excno;
void _ded()
{
    gfxSetScreenFormat(GFX_TOP, GSP_RGB565_OES);
    gfxSetDoubleBuffering(GFX_TOP, false);
    gfxw_swapbuffer();
    gfxw_swapbuffer();
    gfxw_flushbuffer();
    
    puts("\e[0m\n\n- The application has crashed\n\n");
    
    try
    {
        throw;
    }
    catch(std::exception &e)
    {
        printf("std::exception: %s\n", e.what());
    }
    catch(const string& str)
    {
        printf("string: '%s'\n", str.c_str());
    }
    catch(const char* str)
    {
        printf("cstr: '%s'\n", str);
    }
    catch(Result res)
    {
        printf("Result: %08X\n", res);
        //NNERR(res);
    }
    catch(int e)
    {
        printf("(int) %i\n", e);
    }
    catch(...)
    {
        puts("<unknown exception>");
    }
    
    puts("\n");
    
    hangmacro();
    
    killswitch:
    longjmp(__exc, 1);
}


static int haznet = 0;
int checkwifi()
{
    haznet = 0;
    u32 wifi = 0;
    hidScanInput();
    if(hidKeysHeld() == (KEY_SELECT | KEY_START)) return 0;
    if(ACU_GetWifiStatus(&wifi) >= 0 && wifi) haznet = 1;
    return haznet;
}

void drawtext(const char* wat, int sx, int sy, int color = 0, int scx = 1, int scy = 1, int esx = 0xFFFF, int esy = 0xFFFF)
{
    rendertext(wat, sx, sy, esx, esy, -1, color, scx, scy);
}

void drawstring(const string str, int sx, int sy, int color = 0, int scx = 1, int scy = 1, int esx = 0xFFFF, int esy = 0xFFFF)
{
    rendertext(str.c_str(), sx, sy, esx, esy, str.length(), color, scx, scy);
}

Snowflake ss = 0ULL;
Snowflake sc = 0ULL;
static WebSocketWSS* wsa = nullptr;
static DiscordWSA* dsa = nullptr;
static DiscordAPI* dapi = nullptr;

void WSAHandler(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg* arg)
{
    dsa->WSCallback(ctx, arg);
}

void wsahandler_lol(string& evt, JSONObject& d)
{
    dapi->DispatchMessage(evt, d);
}



int main()
{
    gfxw_init();
    inittext();
    
    
    Result res = 0;
    u32 kHeld = 0;
    u32 kDown = 0;
    u32 kUp = 0;
    
    s16 scroll = 0;
    
    u64 currtick = svcGetSystemTick();
    u64 prevtick = currtick;
    
    circlePosition cpos;
    touchPosition touch;
    
    Ini ini;
    string token;
    
    if((__excno = setjmp(__exc))) goto killswitch;
    
#ifdef _3DS
    std::set_unexpected(_ded);
    std::set_terminate(_ded);
#endif
    
    
    consoleInit(GFX_TOP, nullptr);
    
    
    sf2d_set_clear_color((rand() & 0xFF) | ((rand() & 0xFF) << 8) | ((rand() & 0xFF) << 16) | ((rand() & 0xFF) << 24));
    
    osSetSpeedupEnable(1);
    APT_SetAppCpuTimeLimit(30);
    
    res = socInit((u32*)memalign(0x1000, 0x10000), 0x10000);
    res = sslcInit(0);
    
    do
    {
        FILE* f = fopen("/.DiscordCTR.ini", "r");
        if(f <= 0)
        {
            f = fopen("/.DiscordCTR.ini", "w");
            if(f > 0)
            {
                fputs("\n", f);
                fflush(f);
                fclose(f);
            }
        }
        else fclose(f);
        
        
        fstream fo("/.DiscordCTR.ini", fstream::in);
        
        fo >> ini;
        
        fo.close();
    }
    while(0);
    
    token = ini["Auth"]["Token"];
    
    if(!token.length())
    {
        AsyncTextInput::Start
        (
            256, token,
            [&token](const string& res)
            {
                token = res;
            }
        );
        
        while(AsyncTextInput::Running()) svcSleepThread(50e6);
        
        if(token.length())
        {
            ini["Auth"]["Token"] = token;
        }
        else
        {
            throw "No token entered, fatal error";
        }
    }
    
    
    puts("Testing WebSocket shit");
    
    dapi = new DiscordAPI;
    
    dsa = new DiscordWSA(wsahandler_lol);
    wsa = WSHelper::CreateWSS(WSAHandler, "gateway.discord.gg", "?v=5&encoding=json");
    dsa->ws = wsa;
    dsa->token = token;
    dapi->token = token;
    
    
    while(aptMainLoop())
    {
        hidScanInput();
        kHeld = hidKeysHeld();
        kDown = hidKeysDown();
        kUp = hidKeysUp();
        hidCircleRead(&cpos);
        if(kHeld & KEY_TOUCH) hidTouchRead(&touch);
        if(kHeld & KEY_SELECT) break;
        
        
        if(kDown & KEY_START)
        {
            if(wsa->CheckConnect())
            {
                puts("Closing");
                wsa->Close();
            }
            else
            {
                puts("Opening");
                wsa->Open();
            }
        }
        
        if(kDown & KEY_A)
        {
            u32 idx = (((scroll - 8) * -1) >> 4);
            
            if(ss)
            {
                if(sc)
                {
                    AsyncTextInput::Start
                    (
                        2000, "",
                        [&dapi](const string& res)
                        {
                            if(!res.length()) return;
                            DEBUGS("sending message");
                            string dummy;
                            dapi->APIRequest
                            (
                                "POST", "channels/" + to_string(sc) + "/messages", dummy,
                                "{\"content\":" + JSONValue(res).Stringify() + "}"
                            );
                            DEBUG("resp: %s\n================\n", dummy.c_str());
                        }
                    );
                }
                else
                {
                    if(ss == -1ULL)
                    {
                        if(dapi->PMs.size() > idx)
                        {
                            auto it = dapi->PMs.begin();
                            advance(it, idx);
                            sc = it->first;
                        }
                        else ss = 0ULL;
                    }
                    else if(dapi->servers[ss].channels.size() > idx)
                    {
                        auto it = dapi->servers[ss].channels.begin();
                        advance(it, idx);
                        sc = it->first;
                    }
                    else ss = 0ULL;
                }
            }
            else
            {
                if(dapi->servers.size() > idx)
                {
                    auto it = dapi->servers.begin();
                    advance(it, idx);
                    ss = it->first;
                }
                else ss = -1ULL;
            }
            
            scroll = 0;
        }
        
        if(kDown & KEY_Y)
        {
            scroll = 0;
            if(sc) sc = 0ULL;
            else if(ss) ss = 0ULL;
        }
        
        //if(kDown & KEY_Y)
        if(0)
        {
            AsyncTextInput::Start
            (
                16, "hello there!",
                [](const string& res)
                {
                    printf("swkbd res: '%s'\n", res.c_str());
                }
            );
        }
        
        currtick = svcGetSystemTick();
        wsa->Tick();
        if(wsa->CheckConnect()) dsa->Tick((currtick - prevtick) / 288111);
        prevtick = currtick;
        
        
        if(AsyncTextInput::Running())
        {
            svcSleepThread(20e6);
            continue;
        }
        //========[RENDERING ONLY BELOW]========
        
        if(kHeld & (KEY_X | KEY_UP)) scroll += (kHeld & (KEY_L | KEY_R)) ? 8 : 1;
        if(kHeld & (KEY_B | KEY_DOWN)) scroll -= (kHeld & (KEY_L | KEY_R)) ? 8 : 1;
        
        
        //gfxw_startscreen(0);
        
        
        
        //gfxw_endscreen();
        
        
        gfxw_startscreen(1);
        
        s16 py = scroll;
        
        if(ss)
        {
            if(sc)
            {
                if(!dapi->HasChannel(sc))
                {
                    sc = 0ULL;
                }
                else if(ss == -1ULL)
                {
                    drawstring("In a PM with @" + dapi->GetChannel(sc)->name, 0, 0, 0, 2, 2);
                }
                else
                {
                    drawstring("In channel #" + dapi->GetChannel(sc)->name + " on server " + dapi->servers[dapi->GetChannel(sc)->ServerID].name, 0, 0, 0, 2, 2);
                }
            }
            else if(ss == -1ULL)
            {
                drawstring(">", 0, 0, 0, 2, 2);
                for(auto it : dapi->PMs)
                {
                    drawstring(" - @" + it.second.name, 0, py, 0, 2, 2);
                    py += 16;
                    if(py >= 240) break;
                }
                drawstring(" - <Back>", 0, py, 0, 2, 2);
            }
            else if(dapi->servers.count(ss))
            {
                drawstring(">", 0, 0, 0, 2, 2);
                for(auto it : dapi->servers[ss].channels)
                {
                    if(it.second.type == DiscordChannelType::DCT_VOICE)
                        drawstring(" - \x0E" + it.second.name, 0, py, 0, 2, 2);
                    else
                        drawstring(" - #" + it.second.name, 0, py, 0, 2, 2);
                    py += 16;
                    if(py >= 240) break;
                }
                drawstring(" - <Back>", 0, py, 0, 2, 2);
            }
            else
            {
                sc = 0ULL;
                ss = 0ULL;
            }
        }
        else
        {
            drawstring(">", 0, 0, 0, 2, 2);
            for(auto it : dapi->servers)
            {
                drawstring(" - " + it.second.name, 0, py, 0, 2, 2);
                py += 16;
                if(py >= 240) break;
            }
            drawstring(" - <PMs>", 0, py, 0, 2, 2);
        }
        
        if(py < 0) scroll -= py;
        if(scroll > 0) scroll = 0;
        
        gfxw_endscreen();
        
        //gfxw_flushbuffer();
        gfxw_swapbuffer();
        gfxw_wait4vblank();
    }
    
    do
    {
        fstream fo("/.DiscordCTR.ini", fstream::out);
        
        fo << ini << endl;
        
        fo.close();
    }
    while(0);
    
    killswitch:
    
    delete wsa;
    
    sslcExit();
    
    SOCU_ShutdownSockets();
    //SUCU_CloseSockets();
    socExit();
    
    deinittext();
    gfxw_exit();
    
    return 0;
}
