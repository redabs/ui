#include <stdio.h>
#include <stdlib.h>
#include "ui.h"

#define UI_OFFSET_OF(Type, Member) ((size_t) &(((Type *)0)->Member))
#define UI_REBASE(MemberInstance, StructName, MemberName) (StructName *)((unsigned char *)MemberInstance - UI_OFFSET_OF(StructName, MemberName))
#define UI_ARRAYCOUNT(X) (sizeof(X) / sizeof(X[0]))
#define UI_ABORT(Message) (fprintf(stderr, "UI_ASSERT: %s %s:%d \n", Message, \
                                   __FILE__, __LINE__), exit(1))
#define UI_ASSERT(X, Message) if(!(X)) { UI_ABORT(Message); }
#define UI_STACK_PUSH(S, Type) ((S.Index < UI_ARRAYCOUNT(S.Items)) ? \
                                &S.Items[S.Index++] : (UI_ABORT("Stack is full"), (Type *)0))
#define UI_MAX(X, Y) ((X > Y) ? X : Y)
#define UI_MIN(X, Y) ((X < Y) ? X : Y)
#define UI_INT_MAX 0x7fffffff

ui_color UI_COLOR1 = {0x32, 0x30, 0x31, 0xff};
ui_color UI_COLOR0 = {0x3d, 0x3b, 0x3c, 0xff};
ui_color UI_COLOR2 = {0x5e, 0x5a, 0x5a, 0xff};
ui_color UI_COLOR3 = {0x7f, 0x79, 0x79, 0xff};
ui_color UI_COLOR4 = {0xc1, 0xbd, 0xb3, 0xff};
ui_color UI_COLOR_TEXT = {0xee, 0xee, 0xee, 0xff};
ui_color UI_COLOR_HIGHLIGHT = {0x9e, 0xbb, 0x6b, 0xff};

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

ui_command *
UI_PushCommandEx(ui_context *Ctx, int Direction) {
    UI_ASSERT(Ctx->CommandStack.Index2 > (Ctx->CommandStack.Index + 1), "Command stack full");
    ui_command *Result;
    if(Direction == 1) {
        Result = &Ctx->CommandStack.Items[Ctx->CommandStack.Index++];
    } else if(Direction == -1) {
        Result = &Ctx->CommandStack.Items[Ctx->CommandStack.Index2--];
    } else {
        UI_ABORT("Invalid direction");
    }
    return Result;
}

ui_command *
UI_PushCommand(ui_context *Ctx) {
    return UI_PushCommandEx(Ctx, Ctx->ActiveBlock->Direction);
}

void
UI_Begin(ui_context *Ctx) {
    Ctx->TextBufferTop = 0;
    Ctx->CommandStack.Index = 0;
    Ctx->CommandStack.Index2 = UI_COMMAND_MAX - 1;
    Ctx->CommandRefStack.Index = 0;
    Ctx->CmdIndex = 0;
    Ctx->CmdRefIndex = 0;
}

void
UI_End(ui_context *Ctx) {
    Ctx->MouseEvent.Active = 0;
    Ctx->MouseScroll = 0;

    if(!Ctx->SomethingIsHot) {
        Ctx->Hot = 0;
    }
    Ctx->SomethingIsHot = 0;

    if(Ctx->PopUp.MarkedForDeath) {
        Ctx->PopUp.ID = 0;
        Ctx->PopUp.MarkedForDeath = 0;
    }

    UI_SortCommandRefs(Ctx->CommandRefStack.Items, 0, Ctx->CommandRefStack.Index - 1);
}

int
UI_NextCommand(ui_context *Ctx, ui_command **Command) {
    for(; Ctx->CmdRefIndex < Ctx->CommandRefStack.Index; Ctx->CmdRefIndex++) {
        ui_command_ref *Ref = &Ctx->CommandRefStack.Items[Ctx->CmdRefIndex];
        int CmdCount = 1;
        if(Ref->Target->Type == UI_COMMAND_BLOCK) {
            CmdCount = Ref->Target->Command.Block.CommandCount;
            /* The command at index 0 is of type UI_COMMNAD_BLOCK
             * if the reference command points to a block, don't send it
             * to the user. */
            Ctx->CmdIndex += (Ctx->CmdIndex == 0);
        }
        if(Ctx->CmdIndex < CmdCount) {
            ui_command *Cmd = Ref->Target + (Ctx->CmdIndex * Ref->Target->Command.Block.Direction);
            *Command = Cmd;
            Ctx->CmdIndex++;
            return 1;
        }
        Ctx->CmdIndex = 0;
    }

    return 0;
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
UI_FloatWindowToTop(ui_context *Ctx, int x, int y) {
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

void
UI_MousePosition(ui_context *Ctx, int x, int y) {
    Ctx->MousePosPrev = UI_V2(Ctx->MousePos.x, Ctx->MousePos.y);
    Ctx->MousePos = UI_V2(x, y);
}

void
UI_MouseWheel(ui_context *Ctx, int DeltaY) {
    Ctx->MouseScroll += -DeltaY;
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
        if(Ctx->PopUp.ID && !UI_PointInsideRect(Ctx->PopUp.Rect, UI_V2(x, y))) {
            Ctx->PopUp.MarkedForDeath = 1;
            UI_FloatWindowToTop(Ctx, x, y);
        } else if(!Ctx->PopUp.ID) {
            UI_FloatWindowToTop(Ctx, x, y);
        }
    }
}

int
UI_OverWindow(ui_context *Ctx, ui_window *Window) {
    for(int i = Ctx->WindowStack.Index - 1; i >= 0; i--) {
        if(UI_PointInsideRect(Ctx->WindowDepthOrder[i]->Rect, Ctx->MousePos)) {
            return (Ctx->WindowDepthOrder[i]->ID == Window->ID);
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

    if(!Ctx->Active) {
        if(Ctx->PopUp.ID && UI_PointInsideRect(Ctx->PopUp.Rect, Ctx->MousePos)) {
            Ctx->Hot = Ctx->PopUp.ID;
            Ctx->SomethingIsHot = 1;
        } else if(UI_PointInsideRect(Rect, Ctx->MousePos) &&
                  UI_OverWindow(Ctx, Ctx->WindowSelected)) {
            Ctx->Hot = ID;
            Ctx->SomethingIsHot = 1;
        }
    }

    return Result;
}

/* Draw */

void
UI_PushClipRect(ui_context *Ctx, ui_rect Rect) {
    ui_command *Cmd = UI_PushCommand(Ctx);
    Cmd->Type = UI_COMMAND_PUSH_CLIP;
    Cmd->Command.Clip.Rect = Rect;
}

void
UI_PopClipRect(ui_context *Ctx) {
    ui_command *Cmd = UI_PushCommand(Ctx);
    Cmd->Type = UI_COMMAND_POP_CLIP;
}

void
UI_DrawRect(ui_context *Ctx, ui_rect Rect, ui_color Color) {
    ui_command *Cmd = UI_PushCommand(Ctx);
    Cmd->Type = UI_COMMAND_RECT;
    Cmd->Command.Rect.Rect = Rect;
    Cmd->Command.Rect.Color = Color;
}

void
UI_DrawIcon(ui_context *Ctx, int ID, ui_rect Rect, ui_color Color) {
    ui_command *Cmd = UI_PushCommand(Ctx);
    Cmd->Type = UI_COMMAND_ICON;
    Cmd->Command.Icon.Rect = Rect;
    Cmd->Command.Icon.Color = Color;
    Cmd->Command.Icon.ID = ID;
}

/* All parameter of the passed rect is not necessarily used.
 * Rect (x, y, width, height)
 * UI_TEXT_OPT_ORIGIN : Rect (used, used, unused, unused)
 * UI_TEXT_OPT_CENTER : Rect (used, used, used, used)
 * UI_TEXT_OPT_VERT_CENTER : Rect (used, used, unused, used)
 * The rect returned is the bounding box of the text. */
ui_rect
UI_DrawText(ui_context *Ctx, char *Text, ui_rect Rect, ui_color Color, int Options) {
    ui_rect Result;
    ui_command *Cmd = UI_PushCommand(Ctx);
    Cmd->Type = UI_COMMAND_TEXT;

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

/* Layout */

ui_v2
UI_NextRow(ui_window *Window) {
    ui_v2 Result;
    Result.x = Window->Body.x;
    Window->Cursor.x = Window->Body.x;

    Result.y = Window->Cursor.y - Window->RowHeight;
    Window->Cursor.y -= Window->RowHeight + UI_DEFAULT_PADDING;

    Window->RowHeight = 0;

    return Result;
}

void
UI_Inline(ui_context *Ctx) {
    if(Ctx->WindowSelected->Inline) {
        UI_NextRow(Ctx->WindowSelected);
    }
    Ctx->WindowSelected->Inline ^= 1;
}

ui_v2
UI_AdvanceCursor(ui_window *Window, int x, int y) {
    ui_v2 Result;
    Window->RowHeight = UI_MAX(y, Window->RowHeight);

    if(Window->Inline) {
        Result = UI_V2(Window->Cursor.x, Window->Cursor.y - y);
        Window->Cursor.x += x + UI_DEFAULT_PADDING;
    } else {
        Result = UI_NextRow(Window);
    }

    Result.x += UI_DEFAULT_PADDING;
    Result.y += Window->Body.y + Window->Body.h - UI_DEFAULT_PADDING + Window->Scroll;

    return Result;
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
UI_Window(ui_context *Ctx, char *Name, int x, int y) {
    ui_id ID = UI_Hash(Name, 0);
    ui_window *Window = UI_FindWindow(Ctx, ID);
    if(!Window) {
        Window = UI_STACK_PUSH(Ctx->WindowStack, ui_window);
        Window->ID = ID;
        Window->Rect = UI_Rect(x, y - UI_WINDOW_MIN_HEIGHT, UI_WINDOW_MIN_WIDTH, UI_WINDOW_MIN_HEIGHT);
        Window->Title = UI_Rect(x, y - UI_WINDOW_TITLE_BAR_HEIGHT, Window->Rect.w, UI_WINDOW_TITLE_BAR_HEIGHT);
        Window->Body = UI_Rect(x, y - Window->Rect.h, Window->Rect.w, Window->Rect.h - Window->Title.h);

        Window->ZIndex = Ctx->ZIndexTop++;
        Ctx->WindowDepthOrder[Ctx->WindowStack.Index - 1] = Window;
    }
    Ctx->WindowSelected = Window;
    Window->Cursor = UI_V2(0, 0);

    ui_rect ResizeNotch = 
        UI_Rect(Window->Rect.x + Window->Rect.w - UI_WINDOW_RESIZE_ICON_SIZE,
                Window->Rect.y,
                UI_WINDOW_RESIZE_ICON_SIZE, 
                UI_WINDOW_RESIZE_ICON_SIZE);

    ui_id NotchID = UI_Hash("resize_notch", Window->ID);
    UI_UpdateInputState(Ctx, ResizeNotch, NotchID);
    if(NotchID == Ctx->Active) {
        ui_v2 ControlPoint = UI_V2(ResizeNotch.x + ResizeNotch.w, ResizeNotch.y);

        int NewHeight = UI_MAX(Window->Rect.h - (Ctx->MousePos.y - ControlPoint.y), UI_WINDOW_MIN_HEIGHT);
        int dH = NewHeight - Window->Rect.h;
        int NewWidth = UI_MAX(Window->Rect.w + Ctx->MousePos.x - ControlPoint.x, UI_WINDOW_MIN_WIDTH);
        int dW = NewWidth - Window->Rect.w;
        Window->Rect.y -= dH;
        Window->Rect.h += dH;
        Window->Rect.w += dW;
        Window->Title.w += dW;
        Window->Body.y -= dH;
        Window->Body.h += dH;
        Window->Body.w += dW;
        ResizeNotch.x += dW;
        ResizeNotch.y -= dH;
    }

    UI_UpdateInputState(Ctx, Window->Title, ID);
    if(ID == Ctx->Active) {
        int dX = Ctx->MousePos.x - Ctx->MousePosPrev.x;
        int dY = Ctx->MousePos.y - Ctx->MousePosPrev.y;
        Window->Rect.x += dX;
        Window->Rect.y += dY;
        Window->Title.x += dX;
        Window->Title.y += dY;
        Window->Body.x += dX;
        Window->Body.y += dY;
        ResizeNotch.x += dX;
        ResizeNotch.y += dY;
    }

    /* TODO: Support creating windows while creating another window */
    UI_ASSERT(!Ctx->ActiveBlock, "Can't recursively create command blocks");
    ui_command *Cmd = UI_STACK_PUSH(Ctx->CommandStack, ui_command);
    Cmd->Type = UI_COMMAND_BLOCK;
    Cmd->Command.Block.ZIndex = Window->ZIndex;
    Cmd->Command.Block.Direction = 1;
    Ctx->ActiveBlock = &Cmd->Command.Block;

    ui_command_ref *CmdRef = UI_STACK_PUSH(Ctx->CommandRefStack, ui_command_ref);
    CmdRef->Target = Cmd;
    CmdRef->SortKey = Window->ZIndex;

    UI_DrawRect(Ctx, UI_Rect(Window->Rect.x - UI_WINDOW_BORDER, 
                             Window->Rect.y - UI_WINDOW_BORDER,
                             Window->Rect.w + 2 * UI_WINDOW_BORDER, 
                             Window->Rect.h + 2 * UI_WINDOW_BORDER),
                UI_COLOR3);
    UI_DrawRect(Ctx, Window->Body, UI_COLOR0);

    UI_PushClipRect(Ctx, Window->Title);
    UI_DrawText(Ctx, Name, UI_Rect(Window->Title.x + UI_DEFAULT_PADDING,
                                   Window->Title.y, 
                                   Window->Title.w, Window->Title.h),
                UI_COLOR_TEXT, UI_TEXT_OPT_VERT_CENTER);
    UI_PopClipRect(Ctx);
    
    UI_DrawIcon(Ctx, UI_ICON_RESIZE, ResizeNotch, UI_COLOR4);
    UI_PushClipRect(Ctx, Window->Body);
}

void
UI_EndWindow(ui_context *Ctx) {
    ui_window *Window = Ctx->WindowSelected;
    int HeightOfContent = -Window->Cursor.y;
    ui_id ScrollID = UI_Hash("scroll_bar", Window->ID);
    if(HeightOfContent > Window->Body.h && 
       (UI_OverWindow(Ctx, Window) || Ctx->Active == ScrollID)) {
        int Width = 8;
        ui_rect Track = UI_Rect(Window->Body.x + Window->Body.w - Width,
                                Window->Body.y + UI_WINDOW_RESIZE_ICON_SIZE, 
                                Width, 
                                Window->Body.h - UI_WINDOW_RESIZE_ICON_SIZE);
        ui_rect Slider = UI_Rect(Track.x, Track.y,
                                 Track.w,
                                 (int)(Track.h * (float)Window->Body.h / HeightOfContent));

        int ScrollRange = HeightOfContent - Window->Body.h + UI_DEFAULT_PADDING;

        UI_UpdateInputState(Ctx, Track, ScrollID); 
        if(Ctx->Active == ScrollID) {
            int dY = Ctx->MousePosPrev.y - Ctx->MousePos.y;
            Window->Scroll += (float)dY / (Track.h - Slider.h) * ScrollRange;
        } 
        if(UI_OverWindow(Ctx, Window)) {
            Window->Scroll += Ctx->MouseScroll * 10;
        }
        Window->Scroll = UI_Clamp(Window->Scroll, 0, ScrollRange);

        float N = 1. - (float)Window->Scroll / ScrollRange;
        Slider.y = (Track.h - Slider.h) * N + Track.y;

        UI_DrawRect(Ctx, Track, UI_COLOR1);
        UI_DrawRect(Ctx, Slider, UI_COLOR4);
    }
    
    Ctx->WindowSelected = 0;
    UI_PopClipRect(Ctx);

    /* TODO: Again, this does not work with command blocks inside other command
     * blocks */
    ui_command *End = (Ctx->CommandStack.Items + Ctx->CommandStack.Index);
    ui_command *Start = UI_REBASE(Ctx->ActiveBlock, ui_command, Command.Block);
    Ctx->ActiveBlock->CommandCount = End - Start;
    Ctx->ActiveBlock = 0;
}

/* Widgets */

void
UI_BeginPopUp(ui_context *Ctx) {
    Ctx->PausedBlock = Ctx->ActiveBlock;
    ui_command *Cmd = UI_PushCommandEx(Ctx, -1);
    Cmd->Type = UI_COMMAND_BLOCK;
    Cmd->Command.Block.ZIndex = UI_INT_MAX;
    Cmd->Command.Block.Direction = -1;
    Ctx->ActiveBlock = &Cmd->Command.Block;

    ui_command_ref *CmdRef = UI_STACK_PUSH(Ctx->CommandRefStack, ui_command_ref);
    CmdRef->Target = Cmd;
    CmdRef->SortKey = UI_INT_MAX;
}

void
UI_EndPopUp(ui_context *Ctx) {
    ui_command *End = Ctx->CommandStack.Items + Ctx->CommandStack.Index2;
    ui_command *Start = UI_REBASE(Ctx->ActiveBlock, ui_command, Command.Block);
    Ctx->ActiveBlock->CommandCount = Start - End; /* Direction is -1 */
    Ctx->ActiveBlock = Ctx->PausedBlock;
}

int
UI_Number(ui_context *Ctx, char *Name, float Step, float *Value) {
    float OldValue = *Value;

    ui_id ID = UI_Hash(Name, 0);

    int ButtonWidth = Ctx->TextHeight + 4;
    int ButtonHeight = ButtonWidth;
    int NumberFieldWidth = Ctx->TextWidth("0") * 10;
    int ContainerWidth = ButtonWidth * 2 + NumberFieldWidth; 
    ui_v2 Dest = UI_AdvanceCursor(Ctx->WindowSelected, ContainerWidth, ButtonHeight);
    ui_rect ContainerRect = UI_Rect(Dest.x, Dest.y, ContainerWidth, ButtonHeight);

    int x = ContainerRect.x;
    ui_rect DecRect = UI_Rect(x, ContainerRect.y, ButtonWidth, ContainerRect.h);
    x += ButtonWidth;
    ui_rect NumberFieldRect = UI_Rect(x, ContainerRect.y, NumberFieldWidth, ContainerRect.h);
    x += NumberFieldRect.w;
    ui_rect IncRect = UI_Rect(x, ContainerRect.y, ButtonWidth, ContainerRect.h);

    if(UI_UpdateInputState(Ctx, IncRect, UI_Hash("inc_button", ID)) == UI_INTERACTION_PRESS) {
        *Value += Step;
    } else if(UI_UpdateInputState(Ctx, DecRect, UI_Hash("dec_button", ID)) == UI_INTERACTION_PRESS) {
        *Value -= Step;
    }

    UI_DrawRect(Ctx, DecRect, UI_COLOR2);
    UI_DrawText(Ctx, "-", DecRect, UI_COLOR_TEXT, UI_TEXT_OPT_CENTER);
    UI_DrawRect(Ctx, NumberFieldRect, UI_COLOR1);
    UI_DrawRect(Ctx, IncRect, UI_COLOR2);
    UI_DrawText(Ctx, "+", IncRect, UI_COLOR_TEXT, UI_TEXT_OPT_CENTER);
    UI_DrawText(Ctx, UI_PushNumberString(Ctx, *Value), NumberFieldRect, UI_COLOR_TEXT, UI_TEXT_OPT_CENTER);

    return (OldValue != *Value);
}

int
UI_Slider(ui_context *Ctx, char *Name, float Low, float High, float *Value) {
    float OldValue = *Value;

    int SliderTrackWidth = Ctx->TextWidth("0") * 15;
    int SliderTrackHeight = Ctx->TextHeight + 4;
    ui_v2 Dest = UI_AdvanceCursor(Ctx->WindowSelected, SliderTrackWidth, SliderTrackHeight);
    ui_rect SliderTrackRect = UI_Rect(Dest.x, Dest.y, SliderTrackWidth, SliderTrackHeight);

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

    UI_DrawRect(Ctx, SliderTrackRect, UI_COLOR1);

    float t = (*Value - Low) / (High - Low);
    float Left = SliderTrackRect.x + 1;
    float Right = SliderTrackRect.x + SliderTrackRect.w - SliderWidth - 1;
    float x = (1.f - t) * Left + t * Right;
    ui_rect SliderRect = UI_Rect(x, SliderTrackRect.y + 1, SliderWidth, SliderHeight);

    UI_DrawRect(Ctx, SliderRect, UI_COLOR_HIGHLIGHT);
    UI_DrawText(Ctx, UI_PushNumberString(Ctx, *Value), SliderTrackRect, UI_COLOR_TEXT, UI_TEXT_OPT_CENTER);

    return (*Value != OldValue);

}

int
UI_CheckBox(ui_context *Ctx, char *Label, int DrawLabel, int *ValueOut) {
    int OldValue = *ValueOut;
    ui_id ID = UI_Hash(Label, 0);
    int Height = Ctx->TextHeight + 2;
    int Width = Height;
    int TextWidth;
    if(DrawLabel) {
        TextWidth = Ctx->TextWidth(Label);
        Width += UI_DEFAULT_PADDING + TextWidth;
    }

    ui_v2 Dest = UI_AdvanceCursor(Ctx->WindowSelected, Width, Height);

    ui_rect Clickable = UI_Rect(Dest.x, Dest.y, Width, Height);
    int Interaction = UI_UpdateInputState(Ctx, Clickable, ID);
    if(Interaction == UI_INTERACTION_PRESS) {
        *ValueOut = *ValueOut != 0 ? 0 : 1;
    }

    ui_rect CheckBox = UI_Rect(Dest.x, Dest.y, Height, Height);
    UI_DrawRect(Ctx, CheckBox, UI_COLOR1);
    if(*ValueOut != 0) {
        int Margin = 4;
        UI_DrawRect(Ctx, 
                    UI_Rect(CheckBox.x + Margin, CheckBox.y + Margin, 
                            CheckBox.w - 2 * Margin, CheckBox.h - 2 * Margin),
                    UI_COLOR_HIGHLIGHT);
    }

    if(DrawLabel) {
        ui_rect TextRect = UI_Rect(Dest.x + CheckBox.w + UI_DEFAULT_PADDING, 
                                   Dest.y, TextWidth, Height);
        UI_DrawText(Ctx, Label, TextRect, UI_COLOR_TEXT, UI_TEXT_OPT_CENTER);
    }

    return (*ValueOut != OldValue);
}

int
UI_Button(ui_context *Ctx, char *Label) {
    ui_id ID = UI_Hash(Label, Ctx->WindowSelected->ID);

    int ButtonHeight = Ctx->TextHeight + 2;
    ui_v2 Dest = UI_AdvanceCursor(Ctx->WindowSelected, UI_BUTTON_WIDTH, ButtonHeight);

    ui_rect BorderRect = UI_Rect(Dest.x, Dest.y, UI_BUTTON_WIDTH, ButtonHeight);
    ui_rect InnerRect = UI_Rect(BorderRect.x + 1, BorderRect.y + 1, BorderRect.w - 2, BorderRect.h - 2);

    int Interaction = UI_UpdateInputState(Ctx, BorderRect, ID);

    ui_color Color = UI_COLOR2;
    if(Ctx->Hot == ID) {
        Color = UI_COLOR_HIGHLIGHT;
    } else if(Ctx->Active == ID) {
        Color = UI_COLOR0;
    }

    UI_DrawRect(Ctx, BorderRect, UI_COLOR2);
    UI_DrawRect(Ctx, InnerRect, Color);
    UI_DrawText(Ctx, Label, BorderRect, UI_COLOR_TEXT, UI_TEXT_OPT_CENTER);

    return Interaction;
}

void
UI_Text(ui_context *Ctx, char *Text, ui_color Color) {
    ui_v2 Dest = UI_AdvanceCursor(Ctx->WindowSelected, Ctx->TextWidth(Text), Ctx->TextHeight);
    UI_DrawText(Ctx, Text, UI_Rect(Dest.x, Dest.y, 0, 0), Color, UI_TEXT_OPT_ORIGIN);
}

int
UI_Dropdown(ui_context *Ctx, char *Name, char **Items, unsigned int ItemCount, unsigned int Stride, int *IndexOut) {
    int Result = 0;
    ui_id ID = UI_Hash(Name, Ctx->WindowSelected->ID);
    ui_id MenuID = UI_Hash("dropdown_menu", ID);

    int Height = Ctx->TextHeight + 2;
    int Width = UI_DROPDOWN_WIDTH + UI_DEFAULT_PADDING + Height;
    ui_v2 Dest = UI_AdvanceCursor(Ctx->WindowSelected, Width, Height);

    ui_rect PreviewBox = UI_Rect(Dest.x, Dest.y, UI_DROPDOWN_WIDTH - Height, Height);
    ui_rect Button = UI_Rect(PreviewBox.x + PreviewBox.w, Dest.y, Height, Height);
    ui_rect Clickable = UI_Rect(PreviewBox.x, PreviewBox.y,  PreviewBox.w + Button.w, PreviewBox.h);

    int ItemHeight = Ctx->TextHeight + 2;
    float MaxVisibleItems = 7.5;
    int MenuHeight = UI_MAX(UI_MIN(ItemHeight * MaxVisibleItems, ItemHeight * ItemCount), ItemHeight);
    if(UI_UpdateInputState(Ctx, Clickable, ID) == UI_INTERACTION_PRESS) {
        if(Ctx->PopUp.ID == ID) {
            Ctx->PopUp.ID = 0;
        } else {
            Ctx->PopUp.ID = MenuID;
            Ctx->DropdownScroll = 0;
        }
    }

    if(Ctx->PopUp.ID == MenuID) {
        ui_rect Menu = UI_Rect(Clickable.x, Clickable.y - MenuHeight, Width, MenuHeight);
        int MenuInteraction = UI_UpdateInputState(Ctx, Menu, MenuID);
        int SelectedItemIndex = -1;
        if(Ctx->Hot == MenuID) {
            int ScrollSpeed = 6;
            int ScrollRange = ItemHeight * ItemCount - MenuHeight;
            Ctx->DropdownScroll += Ctx->MouseScroll * ScrollSpeed;
            Ctx->MouseScroll = 0;
            Ctx->DropdownScroll = UI_Clamp(Ctx->DropdownScroll, 0, ScrollRange);
            SelectedItemIndex = (Ctx->DropdownScroll + Clickable.y - Ctx->MousePos.y) / ItemHeight;
        } else if(MenuInteraction == UI_INTERACTION_PRESS) {
            Result = 1;
            SelectedItemIndex = (Ctx->DropdownScroll + Clickable.y - Ctx->MousePos.y) / ItemHeight;
            *IndexOut = SelectedItemIndex;
        }

        UI_BeginPopUp(Ctx);
        Ctx->PopUp.Rect = Menu;
        UI_PushClipRect(Ctx, Menu);
        UI_DrawRect(Ctx, Menu, UI_COLOR1);

        ui_v2 Cursor = UI_V2(Menu.x, Clickable.y - ItemHeight + Ctx->DropdownScroll);
        char **It = Items;
        for(int i = 0; i < ItemCount; i++) {
            ui_rect Item = UI_Rect(Cursor.x, Cursor.y, Menu.w, ItemHeight);
            UI_DrawText(Ctx, *It, UI_Rect(Item.x + UI_DEFAULT_PADDING, Item.y, Item.w, Item.h), 
                        (i == SelectedItemIndex) ? UI_COLOR_HIGHLIGHT : UI_COLOR_TEXT, UI_TEXT_OPT_VERT_CENTER);
            Cursor.y -= ItemHeight;
            It = (char **)((char *)It + Stride);
        }
        UI_PopClipRect(Ctx);

        if(UI_UpdateInputState(Ctx, Menu, ID) == UI_INTERACTION_PRESS_AND_RELEASED) {

        }
        UI_EndPopUp(Ctx);
    }

    UI_DrawRect(Ctx, PreviewBox, UI_COLOR1);
    UI_DrawRect(Ctx, Button, UI_COLOR2);

    UI_DrawText(Ctx, "v", Button, UI_COLOR_TEXT, UI_TEXT_OPT_CENTER);
    UI_PushClipRect(Ctx, PreviewBox);
    ui_rect TextP = PreviewBox;
    TextP.x += UI_DEFAULT_PADDING;
    UI_DrawText(Ctx, *(char **)((char *)Items + Stride * (*IndexOut)), TextP, UI_COLOR_TEXT, UI_TEXT_OPT_VERT_CENTER);
    UI_PopClipRect(Ctx);

    return Result;
}

