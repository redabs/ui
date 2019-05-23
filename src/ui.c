#include <stdio.h>
#include <stdlib.h>
#include "ui.h"

#define UI_ARRAYCOUNT(X) (sizeof(X) / sizeof(X[0]))
#define UI_ABORT(Message) (fprintf(stderr, "UI_ASSERT: %s %s:%d \n", Message, \
                                   __FILE__, __LINE__), exit(1))
#define UI_ASSERT(X, Message) if(!(X)) { UI_ABORT(Message); }
#define UI_STACK_PUSH(S, Type) ((S.Index < UI_ARRAYCOUNT(S.Items)) ? \
                                &S.Items[S.Index++] : (UI_ABORT("Stack is full"), (Type *)0))
#define UI_MAX(X, Y) ((X > Y) ? X : Y)
#define UI_MIN(X, Y) ((X < Y) ? X : Y)
#define UI_INT_MAX 0x7fffffff

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

ui_v2
UI_RectCenter(ui_rect Rect) {
    ui_v2 Result;
    Result.x = Rect.x + Rect.w / 2;
    Result.y = Rect.y + Rect.h / 2;
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

/* Begin/End */

void
UI_Begin(ui_context *Ctx) {
    Ctx->TextBufferTop = 0;
    Ctx->CommandStack.Index = 0;
    Ctx->CommandRefStack.Index = 0;
}

int
UI_PartitionCommandRefs(ui_command_ref *CmdRefs, int Low, int High) {
#define UI_SWAP(A, B) ui_command_ref T = A; A = B; B = T;
    ui_command_ref Pivot = CmdRefs[High];
    int i = Low;
    for(int j = Low; j < High; j++) {
        if(CmdRefs[j].SortKey < Pivot.SortKey) {
            UI_SWAP(CmdRefs[i], CmdRefs[j]);
            i++;
        }
    }
    UI_SWAP(CmdRefs[i], CmdRefs[High]);
#undef UI_SWAP
    return i;
}

void
UI_SortCommandRefs(ui_command_ref *CmdRefs, int Low, int High) {
    if(Low < High) {
        int P = UI_PartitionCommandRefs(CmdRefs, Low, High);
        UI_SortCommandRefs(CmdRefs, Low, P - 1);
        UI_SortCommandRefs(CmdRefs, P + 1, High);
    }
}

void
UI_End(ui_context *Ctx) {
    Ctx->MouseEvent.Active = 0;
    Ctx->PopUp.WasCreatedThisFrame = 0;
    if(!Ctx->SomethingIsHot) {
        Ctx->Hot = 0;
    }
    Ctx->SomethingIsHot = 0;

    UI_SortCommandRefs(Ctx->CommandRefStack.Items, 0, Ctx->CommandRefStack.Index - 1);
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


    if(EventType == UI_MOUSE_PRESSED) {
        for(int i = Ctx->WindowStack.Index - 1; i >= 0; i--) {
            ui_window *Window = Ctx->WindowDepthOrder[i];
            if(UI_PointInsideRect(Window->Rect, UI_V2(x, y))) {
                if(Window->ZIndex < (Ctx->ZIndexTop - 1)) {
                    /* Update z-index for rendering */
                    Window->ZIndex = Ctx->ZIndexTop++;
                    /* Update depth stacking order for hit detection */
                    for(int j = i; j < Ctx->WindowStack.Index - 1; j++) {
                        Ctx->WindowDepthOrder[j] = Ctx->WindowDepthOrder[j + 1];
                    }
                    Ctx->WindowDepthOrder[Ctx->WindowStack.Index - 1] = Window;
                }
                break;
            }
        }
    }
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

    if(!Ctx->Active &&
       UI_PointInsideRect(Rect, Ctx->MousePos) &&
       UI_OverWindow(Ctx, Ctx->WindowSelected)) {
        Ctx->Hot = ID;
        Ctx->SomethingIsHot = 1;
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
UI_DrawRectEx(ui_context *Ctx, ui_rect Rect, ui_color Color, int Clip) {
    ui_command *Cmd = UI_STACK_PUSH(Ctx->CommandStack, ui_command);
    Cmd->Type = UI_COMMAND_RECT;
    Cmd->Command.Rect.Rect = Rect;
    Cmd->Command.Rect.Color = Color;
    Cmd->Clip = Clip;
}

void
UI_DrawRect(ui_context *Ctx, ui_rect Rect, ui_color Color) {
    UI_DrawRectEx(Ctx, Rect, Color, 1);
}

void
UI_DrawIcon(ui_context *Ctx, int ID, ui_rect Rect, ui_color Color) {
    ui_command *Cmd = UI_STACK_PUSH(Ctx->CommandStack, ui_command);
    Cmd->Type = UI_COMMAND_ICON;
    Cmd->Command.Icon.Rect = Rect;
    Cmd->Command.Icon.Color = Color;
    Cmd->Command.Icon.ID = ID;
}

ui_rect
UI_DrawTextEx(ui_context *Ctx, char *Text, ui_rect Rect, ui_color Color, int Options, int Free) {
    ui_rect Result;
    ui_command *Cmd = UI_STACK_PUSH(Ctx->CommandStack, ui_command);
    Cmd->Type = UI_COMMAND_TEXT;

    if(Free) {
        ui_command_ref *CmdRef = UI_STACK_PUSH(Ctx->CommandRefStack, ui_command_ref);
        CmdRef->SortKey = UI_INT_MAX;
        CmdRef->Target = Cmd;
    }

    int TextWidth = Ctx->TextWidth(Text); 
    switch(Options) {
        case UI_TEXT_OPT_ORIGIN: {
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

/* All parameter of the passed rect is not necessarily used.
 * Rect (x, y, width, height)
 * UI_TEXT_OPT_ORIGIN : Rect (used, used, unused, unused)
 * UI_TEXT_OPT_CENTER : Rect (used, used, used, used)
 * UI_TEXT_OPT_VERT_CENTER : Rect (used, used, unused, used)
 * The rect returned is the bounding box of the text. */
ui_rect
UI_DrawText(ui_context *Ctx, char *Text, ui_rect Rect, ui_color Color, int Options) {
    return UI_DrawTextEx(Ctx, Text, Rect, Color, Options, 0);
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
    ui_id ID = UI_Hash(Name, 0);
    ui_window *Window = UI_FindWindow(Ctx, ID);
    if(!Window) {
        Window = UI_STACK_PUSH(Ctx->WindowStack, ui_window);
        Window->ID = ID;
        Window->Rect = UI_Rect(100, 100, UI_WINDOW_MIN_WIDTH, UI_WINDOW_MIN_HEIGHT);
        Window->ZIndex = Ctx->ZIndexTop++;
        Ctx->WindowDepthOrder[Ctx->WindowStack.Index - 1] = Window;
    }
    Ctx->WindowSelected = Window;

    ui_rect ResizeNotch = 
        UI_Rect(Window->Rect.x + Window->Rect.w - UI_WINDOW_RESIZE_ICON_SIZE,
                Window->Rect.y,
                UI_WINDOW_RESIZE_ICON_SIZE, 
                UI_WINDOW_RESIZE_ICON_SIZE);

    ui_id NotchID = UI_Hash("resize_notch", Window->ID);
    UI_UpdateInputState(Ctx, ResizeNotch, NotchID);
    if(NotchID == Ctx->Active) {
        /* Let the center of the notch be the control point  */
        ui_v2 ControlPoint = UI_RectCenter(ResizeNotch);

        int NewHeight = UI_MAX(Window->Rect.h - (Ctx->MousePos.y - ControlPoint.y), UI_WINDOW_MIN_HEIGHT);
        int dH = NewHeight - Window->Rect.h;
        Window->Rect.y -= dH;
        Window->Rect.h = NewHeight;

        Window->Rect.w  = UI_MAX(Window->Rect.w + Ctx->MousePos.x - ControlPoint.x, UI_WINDOW_MIN_WIDTH);

        /* Recompute ResizeNotch */
        ResizeNotch = UI_Rect(Window->Rect.x + Window->Rect.w - UI_WINDOW_RESIZE_ICON_SIZE,
                              Window->Rect.y,
                              UI_WINDOW_RESIZE_ICON_SIZE, 
                              UI_WINDOW_RESIZE_ICON_SIZE);
    }

    ui_rect TitleRect = UI_ComputeWindowTitleRect(Window->Rect);
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

    /* TODO: Support creating windows while creating another window */
    UI_ASSERT(!Ctx->ActiveBlock, "Can't recursively create command blocks");
    ui_command *Cmd = UI_STACK_PUSH(Ctx->CommandStack, ui_command);
    Cmd->Type = UI_COMMAND_BLOCK;
    Cmd->Command.Block.ZIndex = Window->ZIndex;
    Ctx->ActiveBlock = &Cmd->Command.Block;

    ui_command_ref *CmdRef = UI_STACK_PUSH(Ctx->CommandRefStack, ui_command_ref);
    CmdRef->Target = Cmd;
    CmdRef->SortKey = Window->ZIndex;

    UI_DrawRect(Ctx, Window->Rect, UI_BLACK);
    UI_DrawRect(Ctx, ContentRect, UI_GRAY0);
    UI_PushClipRect(Ctx, TitleRect);
    UI_DrawText(Ctx, Name, TitleRect, UI_WHITE, UI_TEXT_OPT_VERT_CENTER);
    UI_PopClipRect(Ctx);
    UI_DrawIcon(Ctx, UI_ICON_RESIZE, ResizeNotch, UI_BLACK);
    UI_PushClipRect(Ctx, UI_Rect(ContentRect.x + UI_WINDOW_CONTENT_PADDING,
                                 ContentRect.y + UI_WINDOW_CONTENT_PADDING,
                                 ContentRect.w - 2 * UI_WINDOW_CONTENT_PADDING,
                                 ContentRect.h - 2 * UI_WINDOW_CONTENT_PADDING));
}

void
UI_EndWindow(ui_context *Ctx) {
    Ctx->WindowSelected = 0;
    UI_PopClipRect(Ctx);
    /* TODO: Again, this does not work with command blocks inside other command
     * blocks */
    ui_command *End = (Ctx->CommandStack.Items + Ctx->CommandStack.Index);
    ui_command *Start = Ctx->CommandRefStack.Items[Ctx->CommandRefStack.Index - 1].Target;
    Ctx->ActiveBlock->CommandCount = End - Start;
    Ctx->ActiveBlock = 0;
}

/* Widgets */
int
UI_Number(ui_context *Ctx, float Step, float *Value) {
    float OldValue = *Value;

    int ButtonWidth = Ctx->TextHeight + 4;
    int ButtonHeight = ButtonWidth;
    int NumberFieldWidth = Ctx->TextWidth("0") * 10;
    int ContainerWidth = ButtonWidth * 2 + NumberFieldWidth; 
    UI_AdvanceCursor(Ctx->WindowSelected, ContainerWidth, ButtonHeight);
    ui_rect ContainerRect = 
        UI_Rect(Ctx->WindowSelected->Cursor.x, Ctx->WindowSelected->Cursor.y,
                ContainerWidth, ButtonHeight);

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

    int Interaction = UI_UpdateInputState(Ctx, BorderRect, ID);

    ui_color Color = UI_WHITE;
    if(Ctx->Hot == ID) {
        Color = UI_GRAY1;
    } else if(Ctx->Active == ID) {
        Color = UI_GRAY0;
    }

    UI_DrawRect(Ctx, BorderRect, UI_WHITE);
    UI_DrawRect(Ctx, InnerRect, Color);
    UI_DrawText(Ctx, Label, BorderRect, UI_BLACK, UI_TEXT_OPT_CENTER);

    return Interaction;
}

void
UI_Text(ui_context *Ctx, char *Text, ui_color Color) {
    ui_v2 Cursor = Ctx->WindowSelected->Cursor;
    ui_rect TextRect = UI_DrawText(Ctx, Text, UI_Rect(Cursor.x, Cursor.y - Ctx->TextHeight, 0, 0),
                                   Color, UI_TEXT_OPT_ORIGIN);
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
        UI_DrawRectEx(Ctx, Ctx->PopUp.Rect, UI_WHITE, 0);
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
