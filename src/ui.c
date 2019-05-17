#include <stdio.h>

#include "ui.h"

#define UI_ARRAYCOUNT(X) (sizeof(X) / sizeof(X[0]))
#define UI_ABORT(Message) (fprintf(stderr, "UI_ASSERT: %s %s:%d \n", Message, \
                                   __FILE__, __LINE__), __builtin_trap())
#define UI_ASSERT(X, Message) if(!(X)) { UI_ABORT(Message); }
#define UI_STACK_PUSH(S, Type) ((S.Index < UI_ARRAYCOUNT(S.Items)) ? \
                                &S.Items[S.Index++] : (UI_ABORT("Stack is full"), (Type *)0))
#define UI_MAX(X, Y) ((X > Y) ? X : Y)
#define UI_MIN(X, Y) ((X < Y) ? X : Y)

ui_color UI_WHITE = {255, 255, 255, 255};
ui_color UI_BLACK = {0, 0, 0, 255};
ui_color UI_GRAY0 = {78, 78, 78, 255};
ui_color UI_GRAY1 = {200, 200, 200, 255};
ui_color UI_RED = {255, 0, 0, 255};

/* Util */

ui_id
UI_Hash(char *Str, ui_id Hash) {
    if(!Hash) {
        Hash = 2166136261;
    }
    while(*Str) {
        Hash = (Hash ^ *Str++) * 16777619;
    }
    return Hash;
}

float
UI_Clamp(float x, float a, float b) {
    float Result;
    if(a > b) {
        float t = a;
        a = b;
        b = t;
    }
    Result = UI_MIN(UI_MAX(x, a), b);

    return Result;
}

/* Types */

ui_v2
UI_V2(int x, int y) {
    ui_v2 Result;
    Result.x = x;
    Result.y = y;
    return Result;
}

ui_rect
UI_Rect(int x, int y, int w, int h) {
    ui_rect Result;
    Result.x = x;
    Result.y = y;
    Result.w = w;
    Result.h = h;
    return Result;
}

ui_color
UI_Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    ui_color Result;
    Result.r = r;
    Result.g = g;
    Result.b = b;
    Result.a = a;
    return Result;
}

/* Text Buffering */

char *
UI_PushNumberString(ui_context *Ctx, float Value) {
    char *Dest = Ctx->TextBuffer + Ctx->TextBufferTop;
    int BytesWritten = snprintf(Dest, UI_TEXT_MAX - Ctx->TextBufferTop, "%.02f", Value);
    UI_ASSERT((Ctx->TextBufferTop + BytesWritten + 1) < UI_TEXT_MAX, "Text buffer exceeded");

    Ctx->TextBufferTop += BytesWritten + 1;

    return Dest; 
}

/* User input */

int
UI_PointInsideRect(ui_rect Rect, ui_v2 P) {
    if((P.x >= Rect.x && P.x <= (Rect.x + Rect.w)) &&
       (P.y >= Rect.y && P.y <= (Rect.y + Rect.h))) {
       return 1;
   }
    return 0;  
}

int
UI_RectWasPressed(ui_context *Ctx, ui_rect Rect, int Button) {
    if(Ctx->MouseEvent.Active &&
       Ctx->MouseEvent.Button == Button &&
       Ctx->MouseEvent.Type == UI_MOUSE_PRESSED &&
       UI_PointInsideRect(Rect, Ctx->MouseEvent.P)) {
        return 1;
   }
    return 0;
}

void
UI_MousePosition(ui_context *Ctx, int x, int y) {
    Ctx->MousePosPrev = UI_V2(Ctx->MousePos.x, Ctx->MousePos.y);
    Ctx->MousePos = UI_V2(x, y);
}

void
UI_MouseButton(ui_context *Ctx, int x, int y, int Button, int EventType) {
    /* Don't handle mouse button events for buttons other than the one that is 
     * currently held down. */
    if(Ctx->MouseEvent.Type == UI_MOUSE_PRESSED && Button != Ctx->MouseEvent.Button) {
        return;
    }

    Ctx->MouseEvent.Active = 1;
    Ctx->MouseEvent.P = UI_V2(x, y);
    Ctx->MouseEvent.Button = Button;
    Ctx->MouseEvent.Type = EventType;
}

int
UI_OverWindow(ui_context *Ctx, ui_window *Window) {
    for(int i = Ctx->WindowStack.Index - 1; i >= 0; i--) {
        if(UI_PointInsideRect(Ctx->WindowDepthOrder[i]->Rect, Ctx->MousePos)) {
            if(Ctx->WindowDepthOrder[i]->ID == Window->ID) {
                return 1;
            } else {
                return 0;
            }
        }
    }
    return 0;
}

int
UI_UpdateInputState(ui_context *Ctx, ui_rect Rect, ui_id ID) {
    int Result = 0;

    if(Ctx->Active == ID) {
        if(Ctx->MouseEvent.Active && Ctx->MouseEvent.Type == UI_MOUSE_RELEASED) {
            Ctx->Active = 0;
            if(UI_PointInsideRect(Rect, Ctx->MouseEvent.P)) {
                Result = UI_INTERACTION_PRESS_AND_RELEASED;
            }
        }
    } else if(Ctx->Hot == ID) {
        if(UI_RectWasPressed(Ctx, Rect, UI_MOUSE_LEFT)) {
            Ctx->Active = ID;
            Ctx->Hot = 0;
            Result = UI_INTERACTION_PRESS;
        }
    }

    if(UI_OverWindow(Ctx, Ctx->WindowSelected)) {
        if(UI_PointInsideRect(Rect, Ctx->MousePos) && !Ctx->Active) {
            Ctx->Hot = ID;
            Ctx->SomethingIsHot = 1;
            /* TODO: Move window to top */
        }
    }

    return Result;
}

/* Draw */

void
UI_PushClipRect(ui_context *Ctx, ui_rect Rect) {
    ui_command *Cmd = UI_STACK_PUSH(Ctx->CommandStack, ui_command);
    Cmd->Type = UI_COMMAND_PUSH_CLIP;
    Cmd->Command.Clip.Rect = Rect;
}

void
UI_PopClipRect(ui_context *Ctx) {
    ui_command *Cmd = UI_STACK_PUSH(Ctx->CommandStack, ui_command);
    Cmd->Type = UI_COMMAND_POP_CLIP;
}

void
UI_DrawRect_(ui_context *Ctx, ui_rect Rect, ui_color Color, int Clip) {
    ui_command *Cmd = UI_STACK_PUSH(Ctx->CommandStack, ui_command);
    Cmd->Type = UI_COMMAND_RECT;
    Cmd->Command.Rect.Rect = Rect;
    Cmd->Command.Rect.Color = Color;
    Cmd->Clip = Clip;
}

void
UI_DrawRect(ui_context *Ctx, ui_rect Rect, ui_color Color) {
    UI_DrawRect_(Ctx, Rect, Color, 1);
}

void
UI_DrawIcon(ui_context *Ctx, int ID, ui_rect Rect, ui_color Color) {
    ui_command *Cmd = UI_STACK_PUSH(Ctx->CommandStack, ui_command);
    Cmd->Type = UI_COMMAND_ICON;
    Cmd->Command.Icon.Rect = Rect;
    Cmd->Command.Icon.Color = Color;
    Cmd->Command.Icon.ID = ID;
}

/* All parameter of the passed rect is not necessarily used.
 * Rect (x, y, width, height)
 * UI_TEXT_OPT_NONE : Rect (used, used, unused, unused)
 * UI_TEXT_OPT_CENTER : Rect (used, used, used, used)
 * UI_TEXT_OPT_VERT_CENTER : Rect (used, used, unused, used)
 * The rect returned is the bounding box of the text.
 * */
ui_rect
UI_DrawText(ui_context *Ctx, char *Text, ui_rect Rect, ui_color Color, int Options) {
    ui_rect Result;
    ui_command *Cmd = UI_STACK_PUSH(Ctx->CommandStack, ui_command);
    Cmd->Type = UI_COMMAND_TEXT;

    int TextWidth = Ctx->TextWidth(Text); 
    switch(Options) {
        case UI_TEXT_OPT_NONE: {
            Result = Cmd->Command.Text.Rect = UI_Rect(Rect.x, Rect.y, TextWidth, Ctx->TextHeight);
        } break;
        case UI_TEXT_OPT_CENTER: {
            Result = Cmd->Command.Text.Rect = UI_Rect(Rect.x + (Rect.w - TextWidth) / 2,
                                                      Rect.y + (Rect.h - Ctx->TextHeight) / 2,
                                                      TextWidth, Ctx->TextHeight);
        } break;
        case UI_TEXT_OPT_VERT_CENTER: {
            Result = Cmd->Command.Text.Rect = UI_Rect(Rect.x, Rect.y + (Rect.h - Ctx->TextHeight) / 2,
                                                      TextWidth, Ctx->TextHeight);
        } break;
        default: {
            UI_ABORT("Invalid text option");
        } break;
    }

    Cmd->Command.Text.Color = Color;
    Cmd->Command.Text.Text = Text;
    return Result;
}

/* Layout */

ui_rect
UI_ComputeWindowContentRect(ui_rect Rect) {
    ui_rect Result = UI_Rect(Rect.x + UI_WINDOW_BORDER, Rect.y + UI_WINDOW_BORDER, 
                             Rect.w - 2 * UI_WINDOW_BORDER, Rect.h - 
                             UI_WINDOW_BORDER - UI_WINDOW_TITLE_BAR_HEIGHT);
    return Result;
}

ui_rect
UI_ComputeWindowTitleRect(ui_rect Rect) {
    ui_rect Result = UI_Rect(Rect.x + UI_WINDOW_BORDER + UI_WINDOW_CONTENT_PADDING,
                             Rect.y + Rect.h - UI_WINDOW_TITLE_BAR_HEIGHT,
                             Rect.w - 2 * (UI_WINDOW_BORDER + UI_WINDOW_CONTENT_PADDING),
                             UI_WINDOW_TITLE_BAR_HEIGHT);
    return Result;
}

void
UI_NextRow(ui_window *Window) {
    ui_rect ContentRect = UI_ComputeWindowContentRect(Window->Rect);
    Window->Cursor.x = ContentRect.x + UI_WINDOW_CONTENT_PADDING;
    Window->Cursor.y -= Window->RowHeight + 1;
    Window->RowHeight = 0;
}

void
UI_Inline(ui_context *Ctx) {
    if(Ctx->WindowSelected->Inline) {
        UI_NextRow(Ctx->WindowSelected);
    }
    Ctx->WindowSelected->Inline ^= 1;
}

void
UI_AdvanceCursor(ui_window *Window, int x, int y) {
    Window->RowHeight = UI_MAX(y, Window->RowHeight);

    if(Window->Inline) {
        Window->Cursor.x += x;
    } else {
        UI_NextRow(Window);
    }
}

/* Window */

ui_window *
UI_FindWindow(ui_context *Ctx, ui_id ID) {
    for(int i = 0; i < Ctx->WindowStack.Index; i++) {
        ui_window *W = &Ctx->WindowStack.Items[i];
        if(W->ID == ID) {
            return W;
        }
    }

    return 0;
}

void
UI_Window(ui_context *Ctx, char *Name) {
    static ui_rect R = {100, 100, 360, 300};

    ui_id ID = UI_Hash(Name, 0);
    ui_window *Window = UI_FindWindow(Ctx, ID);
    if(!Window) {
        Window = UI_STACK_PUSH(Ctx->WindowStack, ui_window);
        Window->ID = ID;
        Window->Rect = R;
        Ctx->WindowDepthOrder[Ctx->WindowStack.Index - 1] = Window;
    }
    Ctx->WindowSelected = Window;

    ui_rect TitleRect = UI_ComputeWindowTitleRect(Window->Rect);
    /* Layers!! */
    UI_UpdateInputState(Ctx, TitleRect, ID);

    if(ID == Ctx->Active) {
        Window->Rect.x += Ctx->MousePos.x - Ctx->MousePosPrev.x;
        Window->Rect.y += Ctx->MousePos.y - Ctx->MousePosPrev.y;
        /* Recompute TitleRect */
        TitleRect = UI_ComputeWindowTitleRect(Window->Rect);
    }

    ui_rect ContentRect = UI_ComputeWindowContentRect(Window->Rect); 
    /* Intialize window cursor */
    Window->Cursor = UI_V2(ContentRect.x + UI_WINDOW_CONTENT_PADDING, 
                           ContentRect.y + ContentRect.h - UI_WINDOW_CONTENT_PADDING);

    UI_DrawRect(Ctx, Window->Rect, UI_BLACK);
    UI_DrawRect(Ctx, ContentRect, UI_GRAY0);
    UI_DrawText(Ctx, Name, TitleRect, UI_WHITE, UI_TEXT_OPT_VERT_CENTER);
    UI_PushClipRect(Ctx, ContentRect);
}

void
UI_EndWindow(ui_context *Ctx) {
    Ctx->WindowSelected = 0;
    UI_PopClipRect(Ctx);
}

/* Widgets */
int
UI_Number(ui_context *Ctx, float Step, float *Value) {
    float OldValue = *Value;

    int ButtonWidth = Ctx->TextHeight + 4;
    int ButtonHeight = ButtonWidth;
    int NumberFieldWidth = Ctx->TextWidth("0") * 10;

    ui_rect ContainerRect = UI_Rect(Ctx->WindowSelected->Cursor.x, Ctx->WindowSelected->Cursor.y - ButtonHeight,
                                    ButtonWidth * 2 + NumberFieldWidth, ButtonHeight);
    UI_AdvanceCursor(Ctx->WindowSelected, ContainerRect.w, ContainerRect.h);

    int x = ContainerRect.x;
    ui_rect DecRect = UI_Rect(x, ContainerRect.y, ButtonWidth, ContainerRect.h);
    x += ButtonWidth;
    ui_rect NumberFieldRect = UI_Rect(x, ContainerRect.y, NumberFieldWidth, ContainerRect.h);
    x += NumberFieldRect.w;
    ui_rect IncRect = UI_Rect(x, ContainerRect.y, ButtonWidth, ContainerRect.h);

    if(UI_RectWasPressed(Ctx, IncRect, UI_MOUSE_LEFT)) {
        *Value += Step;
    } else if(UI_RectWasPressed(Ctx, DecRect, UI_MOUSE_LEFT)) {
        *Value -= Step;
    }

    UI_DrawRect(Ctx, DecRect, UI_BLACK);
    UI_DrawText(Ctx, "-", DecRect, UI_WHITE, UI_TEXT_OPT_CENTER);
    UI_DrawRect(Ctx, NumberFieldRect, UI_WHITE);
    UI_DrawRect(Ctx, IncRect, UI_BLACK);
    UI_DrawText(Ctx, "+", IncRect, UI_WHITE, UI_TEXT_OPT_CENTER);
    UI_DrawText(Ctx, UI_PushNumberString(Ctx, *Value), NumberFieldRect, UI_BLACK, UI_TEXT_OPT_CENTER);

    return (OldValue != *Value);
}

int
UI_Slider(ui_context *Ctx, char *Name, float Low, float High, float *Value) {
    float OldValue = *Value;

    int SliderTrackWidth = Ctx->TextWidth("0") * 15;
    int SliderTrackHeight = Ctx->TextHeight + 4;
    UI_AdvanceCursor(Ctx->WindowSelected, SliderTrackWidth, SliderTrackHeight);
    ui_rect SliderTrackRect = UI_Rect(Ctx->WindowSelected->Cursor.x, Ctx->WindowSelected->Cursor.y,
                                      SliderTrackWidth, SliderTrackHeight);

    ui_id ID = UI_Hash(Name, 0);
    UI_UpdateInputState(Ctx, SliderTrackRect, ID);

    *Value = UI_Clamp(*Value, Low, High);

    int SliderWidth = 10;
    int SliderHeight = SliderTrackHeight - 2;

    if(Ctx->Active == ID) {
        float t = (Ctx->MousePos.x - (SliderTrackRect.x + (float)SliderWidth / 2)) / (SliderTrackRect.w - SliderWidth);
        float NewValue = (1.f - t) * Low + t * High;
        *Value = UI_Clamp(NewValue, Low, High);
    }

    UI_DrawRect(Ctx, SliderTrackRect, UI_BLACK);

    float t = (*Value - Low) / (High - Low);
    float Left = SliderTrackRect.x + 1;
    float Right = SliderTrackRect.x + SliderTrackRect.w - SliderWidth - 1;
    float x = (1.f - t) * Left + t * Right;
    ui_rect SliderRect = UI_Rect(x, SliderTrackRect.y + 1, SliderWidth, SliderHeight);

    UI_DrawRect(Ctx, SliderRect, UI_RED);
    UI_DrawText(Ctx, UI_PushNumberString(Ctx, *Value), SliderTrackRect, UI_WHITE, UI_TEXT_OPT_CENTER);

    return (*Value != OldValue);

}

int
UI_Button(ui_context *Ctx, char *Label) {
    ui_id ID = UI_Hash(Label, 0);

    UI_AdvanceCursor(Ctx->WindowSelected, UI_BUTTON_WIDTH, UI_BUTTON_HEIGHT);

    ui_v2 Cursor = Ctx->WindowSelected->Cursor;
    ui_rect BorderRect = UI_Rect(Cursor.x, Cursor.y, UI_BUTTON_WIDTH, UI_BUTTON_HEIGHT);
    ui_rect InnerRect = UI_Rect(BorderRect.x + 1, BorderRect.y + 1, BorderRect.w - 2, BorderRect.h - 2);

    ui_color Color = UI_WHITE;
    if(Ctx->Hot == ID) {
        Color = UI_GRAY1;
    } else if(Ctx->Active == ID) {
        Color = UI_GRAY0;
    }

    UI_DrawRect(Ctx, BorderRect, UI_WHITE);
    UI_DrawRect(Ctx, InnerRect, Color);
    UI_DrawText(Ctx, Label, BorderRect, UI_BLACK, UI_TEXT_OPT_CENTER);

    int Interaction = UI_UpdateInputState(Ctx, BorderRect, ID);

    return Interaction;
}

void
UI_Text(ui_context *Ctx, char *Text, ui_color Color) {
    ui_v2 Cursor = Ctx->WindowSelected->Cursor;
    ui_rect TextRect = UI_DrawText(Ctx, Text, UI_Rect(Cursor.x, Cursor.y - Ctx->TextHeight, 0, 0),
                                   Color, UI_TEXT_OPT_NONE);
    UI_AdvanceCursor(Ctx->WindowSelected, TextRect.w, TextRect.h);
}

void
UI_PopUp(ui_context *Ctx) {
    Ctx->PopUp.Active = 1;
    Ctx->PopUp.WasCreatedThisFrame = 1;
    Ctx->PopUp.Rect = UI_Rect(0, 0, 100, 100);
}

void
UI_DrawPopUp(ui_context *Ctx) {
    if(Ctx->PopUp.Active) {
        UI_DrawRect_(Ctx, Ctx->PopUp.Rect, UI_WHITE, 0);
        if(!Ctx->PopUp.WasCreatedThisFrame) {
            if(Ctx->MouseEvent.Active && Ctx->MouseEvent.Type == UI_MOUSE_PRESSED &&
               Ctx->MouseEvent.Button == UI_MOUSE_LEFT) {
               if(UI_PointInsideRect(Ctx->PopUp.Rect, Ctx->MouseEvent.P)) {
               } else {
                   Ctx->PopUp.Active = 0;
               }
            }
        }
    }
}

void
UI_DebugWindow(ui_context *Ctx) {
    char *Name = "Debug Window";
    ui_id ID = UI_Hash(Name, 0);
    ui_window *W;
    static char Buf0[128];
    static char Buf1[128];
    static char Buf2[128];
    static char Buf3[128];
    static char Buf4[128];

    static int Press, PressAndRelease;
    int Interaction;
    static float Bla;

    UI_Window(Ctx, Name);
    W = UI_FindWindow(Ctx, ID);
    UI_ASSERT(W, "Window should always be found if we've created it");
    static int I = 1;
    if(I) W->Rect.x -= 30;
    I = 0;

    sprintf(Buf3, "Rect (%d, %d, %d, %d)", W->Rect.x, W->Rect.y, W->Rect.w, W->Rect.h);
    UI_Text(Ctx, Buf3, UI_WHITE);

    sprintf(Buf0, "Mouse (%d, %d)", Ctx->MousePos.x, Ctx->MousePos.y);
    UI_Text(Ctx, Buf0, UI_WHITE);

    sprintf(Buf1, "Window ID: %x", W->ID);
    UI_Text(Ctx, Buf1, UI_WHITE);

    UI_Text(Ctx, Buf2, UI_WHITE);

    Interaction = UI_Button(Ctx, "Clicky");
    if(Interaction == UI_INTERACTION_PRESS) {
        Press++;
        UI_PopUp(Ctx);
    } else if(Interaction == UI_INTERACTION_PRESS_AND_RELEASED) {
        PressAndRelease++;
    }
    UI_DrawPopUp(Ctx);


    sprintf(Buf4, "P: %d, P&R: %d", Press, PressAndRelease);
    UI_Text(Ctx, Buf4, UI_WHITE);


    UI_Number(Ctx, 1, &Bla);
    UI_Slider(Ctx, "slider", -10., 10., &Bla);

    sprintf(Buf2, "Hot: %08x, Active: %08x", Ctx->Hot, Ctx->Active);
    UI_EndWindow(Ctx);

}

void
UI_EndFrame(ui_context *Ctx) {
    Ctx->MouseEvent.Active = 0;
    Ctx->PopUp.WasCreatedThisFrame = 0;
    if(!Ctx->SomethingIsHot) {
        Ctx->Hot = 0;
    }
    Ctx->SomethingIsHot = 0;
    Ctx->TextBufferTop = 0;
    Ctx->CommandStack.Index = 0;
}
