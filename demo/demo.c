#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <stdio.h>

#include "ui.h"

typedef uint8_t u8; 
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef s32 b32;
typedef u8 b8;

typedef float f32;
typedef double f64;

#define ASSERT(X) if(!(X)) { printf("ASSERT: %s:%d \n", __FILE__, __LINE__); __builtin_trap(); }
#define ARRAYCOUNT(X) sizeof(X) / sizeof(X[0])
#define STACK(Name, Size, Type) u32 Name##Index = 0; Type Name[Size]
#define MIN(X, Y) (X < Y) ? X : Y
#define MAX(X, Y) (X > Y) ? X : Y

#include "font/Atlas.inl"

#define BUF_SIZE 1024

f32 TexCoordBuf[BUF_SIZE * 8];
f32 VertBuf[BUF_SIZE * 8];
ui_color ColorBuf[BUF_SIZE * 4];
u32 IndexBuf[BUF_SIZE * 6];
u32 BufIndex;

// ClipRects[0] is used for window clip rect
STACK(ClipRects, 32, ui_rect);

s32 WindowWidth = 640;
s32 WindowHeight = 480;


ui_rect
Inside(ui_rect Rect, ui_rect Parent) {
    s32 RMaxX = MIN(Rect.x + Rect.w, Parent.x + Parent.w);
    s32 RMaxY = MIN(Rect.y + Rect.h, Parent.y + Parent.h);
    Rect.x = MAX(Rect.x, Parent.x);
    Rect.y = MAX(Rect.y, Parent.y);
    Rect.w = MAX(0, RMaxX - Rect.x);
    Rect.h = MAX(0, RMaxY - Rect.y);

    return Rect;
}

void
PushClipRect(ui_rect Rect) {
    ASSERT(ClipRectsIndex + 1 < ARRAYCOUNT(ClipRects));
    Rect = Inside(Rect, ClipRects[ClipRectsIndex - 1]);
    ClipRects[ClipRectsIndex++] = Rect;
}

void
PopClipRect() {
    // Don't pop the window clip rect.
    ASSERT(ClipRectsIndex > 1);
    ClipRectsIndex--;
}

void
PushRect(ui_rect Dest, ui_rect Src, ui_color Color) {
    ASSERT(BufIndex < BUF_SIZE);

    Dest = Inside(Dest, ClipRects[ClipRectsIndex - 1]);
    if(Dest.w == 0 ||  Dest.h == 0) {
        return;
    }

    u32 VertIndex = BufIndex * 8;
    u32 ColorIndex = BufIndex * 4;
    u32 ElementIndex = BufIndex * 4;
    u32 Idx = BufIndex * 6;
    BufIndex++;

    // {1, 1}, {0, 1}, {1, 0}, {0, 0}
    VertBuf[VertIndex] = Dest.x + Dest.w;
    VertBuf[VertIndex + 1] = Dest.y + Dest.h;
    VertBuf[VertIndex + 2] = Dest.x;
    VertBuf[VertIndex + 3] = Dest.y + Dest.h;
    VertBuf[VertIndex + 4] = Dest.x + Dest.w;
    VertBuf[VertIndex + 5] = Dest.y;
    VertBuf[VertIndex + 6] = Dest.x;
    VertBuf[VertIndex + 7] = Dest.y;

    f32 x = (f32)Src.x / ATLAS_WIDTH;
    f32 y = (f32)Src.y / ATLAS_HEIGHT;
    f32 w = (f32)Src.w / ATLAS_WIDTH;
    f32 h = (f32)Src.h / ATLAS_HEIGHT;
    TexCoordBuf[VertIndex] = x + w;
    TexCoordBuf[VertIndex + 1] = y + h;
    TexCoordBuf[VertIndex + 2] = x;
    TexCoordBuf[VertIndex + 3] = y + h;
    TexCoordBuf[VertIndex + 4] = x + w;
    TexCoordBuf[VertIndex + 5] = y;
    TexCoordBuf[VertIndex + 6] = x;
    TexCoordBuf[VertIndex + 7] = y;

    ColorBuf[ColorIndex] = Color;
    ColorBuf[ColorIndex + 1] = Color;
    ColorBuf[ColorIndex + 2] = Color;
    ColorBuf[ColorIndex + 3] = Color;

    IndexBuf[Idx] = ElementIndex;
    IndexBuf[Idx + 1] = ElementIndex + 1;
    IndexBuf[Idx + 2] = ElementIndex + 2;
    IndexBuf[Idx + 3] = ElementIndex + 1;
    IndexBuf[Idx + 4] = ElementIndex + 3;
    IndexBuf[Idx + 5] = ElementIndex + 2;
}

u32
AtlasIndex(char C) {
    u32 Index = (C - ' ' + ATLAS_FONT);
    return Index > ARRAYCOUNT(Atlas) ? 0 : Index;
}

s32
TextHeight() {
    return 16;
}

void
DrawText(s32 x, s32 y, ui_color Color, char *Str) {
    s32 CursorX = x;
    for(char *C = Str; *C; C++) {
        if(*C == ' ') {
            CursorX += 5;
            continue;
        }
        ui_rect Src = Atlas[AtlasIndex(*C)];
        Src.h = TextHeight();
        ui_rect Dest = {CursorX, y, Src.w, Src.h};
        PushRect(Dest, Src, Color);
        CursorX += Src.w + 1;
    }
}

s32
TextWidth(char *Str) {
    s32 Result = 0;
    for(char *C = Str; *C; C++) {
        if(*C == ' ') {
            Result += 5;
            continue;
        }
        Result += Atlas[AtlasIndex(*C)].w + 1;
    }
    return Result;
}

void
DrawTextCentered(ui_rect Rect, ui_color Color, char *Str) {
    s32 x = Rect.x + (Rect.w - TextWidth(Str)) / 2;
    s32 y = Rect.y + (Rect.h - TextHeight()) / 2;
    DrawText(x, y, Color, Str);
}

typedef struct {
    char *Title;
    int Cost;
} movie;

int
main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *Window = SDL_CreateWindow("ui demo",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          WindowWidth, WindowHeight,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    ASSERT(Window);
    SDL_GL_CreateContext(Window);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);    
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    GLuint TextureID;
    glGenTextures(1, &TextureID);
    glBindTexture(GL_TEXTURE_2D, TextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, ATLAS_WIDTH, ATLAS_HEIGHT, 0, GL_ALPHA, GL_UNSIGNED_BYTE, AtlasTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    ui_context UIContext = {0};
    UIContext.TextHeight = TextHeight();
    UIContext.TextWidth = TextWidth;

    while(1) {
        SDL_Event Event;
        while(SDL_PollEvent(&Event)) {
            if(Event.type == SDL_QUIT) {
                return 0;
            } else if(Event.type == SDL_WINDOWEVENT) {
                switch(Event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED: {
                        WindowWidth = Event.window.data1;
                        WindowHeight = Event.window.data2;
                        printf("%d, %d \n", WindowWidth, WindowHeight);
                    } break;
                }
            } else if(Event.type == SDL_KEYDOWN) {
                switch(Event.key.keysym.sym) {
                    case SDLK_ESCAPE: {
                        return 0;
                    } break;
                }
            } else if(Event.type == SDL_MOUSEBUTTONDOWN ||
                      Event.type == SDL_MOUSEBUTTONUP ) {
                switch(Event.button.button) {
                    case SDL_BUTTON_LEFT:
                    case SDL_BUTTON_RIGHT:
                    case SDL_BUTTON_MIDDLE: {
                        int Actions[] = {
                            [SDL_MOUSEBUTTONDOWN] = UI_MOUSE_PRESSED,
                            [SDL_MOUSEBUTTONUP] = UI_MOUSE_RELEASED,
                        };
                        int Buttons[] = {
                            [SDL_BUTTON_LEFT] = UI_MOUSE_LEFT,
                            [SDL_BUTTON_RIGHT] = UI_MOUSE_RIGHT,
                            [SDL_BUTTON_MIDDLE] = UI_MOUSE_MIDDLE,
                        };
                        UI_MouseButton(&UIContext, Event.button.x, WindowHeight - Event.button.y, 
                                       Buttons[Event.button.button], Actions[Event.type]);
                    } break;
                }
            } else if(Event.type == SDL_MOUSEWHEEL) {
                UI_MouseWheel(&UIContext, Event.wheel.y);
            }
        }

        {
            int x, y;
            SDL_GetMouseState(&x, &y);
            UI_MousePosition(&UIContext, x, WindowHeight - y);
        }

        glViewport(0, 0, WindowWidth, WindowHeight);
        glScissor(0, 0, WindowWidth, WindowHeight);
        ClipRects[0] = UI_Rect(0, 0, WindowWidth, WindowHeight);
        ClipRectsIndex = 1;
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, WindowWidth, 0, WindowHeight, -1, 1); 
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        ui_color White = {255, 255, 255, 255};

        UI_Begin(&UIContext);

        UI_Window(&UIContext, "Debug Window", 10, WindowHeight);
        ui_window *UIWindow = UI_FindWindow(&UIContext, UI_Hash("Debug Window", 0));
        char Buf[128]; sprintf(Buf, "z-index %d", UIWindow->ZIndex);
        UI_Text(&UIContext, Buf, White);

        {
            static char Buf[128]; sprintf(Buf, "Mouse Pos: (%d, %d)", UIContext.MousePos.x, UIContext.MousePos.y);
            UI_Text(&UIContext, Buf, White);
        }
        {
            static char Buf[128]; sprintf(Buf, "This window's ID: 0x%x", UI_FindWindow(&UIContext, UI_Hash("Debug Window", 0))->ID);
            UI_Text(&UIContext, Buf, White);
        }

        static char ActiveAndHotIDs[128]; 
        UI_Text(&UIContext, ActiveAndHotIDs, White);

        UI_Inline(&UIContext);
        static int V;
        if(UI_Button(&UIContext, "Click me") == UI_INTERACTION_PRESS_AND_RELEASED) {
            V++;
        }

        char blabla[128];
        sprintf(blabla, "%d", V);
        UI_Text(&UIContext, blabla, White);
        UI_Inline(&UIContext);

        static float Value0, Value1;
        UI_Number(&UIContext, "Value1", 1, &Value1);
        UI_Slider(&UIContext, "Value0", 123, -10, &Value0);
        Value0 = (int)Value0;

        movie Movies[] = {
            {"Kill Bill", 150},
            {"2001: A Space Oddysey", 150},
            {"Sunset Limited", 150},
            {"Micmacs", 150},
            {"Requiem for a Dream", 150},
            {"Do the right thing", 150},
            {"The Drop", 150},
            {"Glengarry Glen Ross", 150},
            {"Star Wars: Return of the Jedi", 150},
            {"Scream", 150},
            {"Napoleon Dynamite", 150},
            {"Black Dynamite", 150}
        };

        static int Index;
        if(UI_Dropdown(&UIContext, "Dropdown", &(Movies[0].Title), ARRAYCOUNT(Movies), (char*)&Movies[1].Title - (char*)&Movies[0].Title, &Index)) {
            printf("%s\n", Movies[Index].Title);
        }


        UI_Inline(&UIContext);
        UI_Button(&UIContext, "button 1");
        UI_Button(&UIContext, "button 2");
        UI_Inline(&UIContext);

        UI_Button(&UIContext, "button 3");

        static int Boolean = 1;
        UI_CheckBox(&UIContext, "CheckBox", 1, &Boolean);

        UI_EndWindow(&UIContext);

        UI_End(&UIContext);

        sprintf(ActiveAndHotIDs, "Hot: 0x%x, Active 0x%x, PopUp 0x%x", UIContext.Hot, UIContext.Active, UIContext.PopUp.ID);

        ui_command *Cmd;
        while(UI_NextCommand(&UIContext, &Cmd)) {
            switch(Cmd->Type) {
                case UI_COMMAND_RECT: {
                    PushRect(Cmd->Command.Rect.Rect, Atlas[ATLAS_WHITE], Cmd->Command.Rect.Color);
                } break;
                case UI_COMMAND_TEXT: {
                    DrawText(Cmd->Command.Text.Rect.x, Cmd->Command.Text.Rect.y, 
                             Cmd->Command.Text.Color, Cmd->Command.Text.Text);
                } break;
                case UI_COMMAND_PUSH_CLIP: {
                    PushClipRect(Cmd->Command.Clip.Rect);
                } break;
                case UI_COMMAND_POP_CLIP: {
                    PopClipRect();
                } break;
                case UI_COMMAND_ICON: {
                    int Icons[] = {
                        [UI_ICON_COLLAPSE] = ATLAS_COLLAPSE,
                        [UI_ICON_RESIZE] = ATLAS_RESIZE,
                        [UI_ICON_EXPAND] = ATLAS_EXPAND};
                    PushRect(Cmd->Command.Icon.Rect, Atlas[Icons[Cmd->Command.Icon.ID]], 
                             Cmd->Command.Icon.Color);
                } break;
            }
        }

        glTexCoordPointer(2, GL_FLOAT, 0, TexCoordBuf);
        glVertexPointer(2, GL_FLOAT, 0, VertBuf);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, ColorBuf);
        ASSERT(glGetError() == 0);
        glDrawElements(GL_TRIANGLES, 6 * BufIndex, GL_UNSIGNED_INT, (const GLvoid *)IndexBuf);

        BufIndex = 0;

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();

        SDL_GL_SwapWindow(Window);
    }

    return 0;
}
