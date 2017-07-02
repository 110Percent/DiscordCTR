#include <3ds.h>

#include "grafx/drawer.h"

#include "ctrufont_bin.h"

#include "font.h"

static sf2d_texture* font = 0;

void inittext()
{
    if(font) deinittext();
    
    u32 _1[128 * 128];
    
    int i, j, k;
    for(i = 0; i != 0x100; i++)
        for(j = 0; j != 8; j++)
            for(k = 0; k != 8; k++)
                _1[((i >> 4) << 10) + ((i & 0xF) << 3) + (j << 7) + k] = (ctrufont_bin[(i << 3) + j] & (1 << (~k & 7))) ? -1U : 0;
    
    font = sf2d_create_texture_mem_RGBA8(_1, 128, 128, TEXFMT_RGBA8, SF2D_PLACE_RAM);
}

void deinittext()
{
    if(font) sf2d_free_texture(font);
    font = 0;
}

void rendertext(const char* wat, int sx, int sy, int esx, int esy, int len, int color, int scx, int scy)
{
    if(!font) return;
    
    if(!(color >> 24)) color |= 0xFF000000;
    
    float scxf = scx;
    float scyf = scy;
    
    scx *= 8;
    scy *= 8;
    
    //int i = 0;
    int x = sx;
    int y = sy;
    char c;
    
    int ex = SCREENW;
    int ey = SCREENH;
    
    if(esx < ex) ex = esx;
    if(esy < ey) ey = esy;
    
    ex -= scx;
    ey -= scy;
    
    while(y <= ey)
    {
        while(x <= ex)
        {
            c = *(wat++);
            
            if(!len--) return;
            if(!c && len < 0) return;
            if(c == '\n') break;
            
            sf2d_draw_texture_part_scale_blend(font, x, y, (c & 0xF) << 3, ((c >> 4) & 0xF) << 3, 8, 8, scxf, scyf, color);
            
            x += scx;
        }
        
        x = sx;
        y += scy;
    }
}
