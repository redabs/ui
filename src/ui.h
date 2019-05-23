#ifndef ui_h
#define ui_h

#define UI_WINDOW_MAX 32
#define UI_COMMAND_MAX 1024
#define UI_TEXT_MAX 16384

#define UI_WINDOW_CONTENT_PADDING 5
#define UI_WINDOW_BORDER 2
#define UI_WINDOW_TITLE_BAR_HEIGHT 20
#define UI_WINDOW_MIN_WIDTH 200
#define UI_WINDOW_MIN_HEIGHT 200
#define UI_WINDOW_RESIZE_ICON_SIZE 25

#define UI_BUTTON_WIDTH 80
#define UI_BUTTON_HEIGHT 30

typedef unsigned int ui_id;

enum {
    UI_ICON_RESIZE,
    UI_ICON_COLLAPSE,
    UI_ICON_EXPAND
};

enum {
    UI_COMMAND_PUSH_CLIP,
    UI_COMMAND_POP_CLIP,
    UI_COMMAND_RECT,
    UI_COMMAND_TEXT,
    UI_COMMAND_ICON,
    UI_COMMAND_BLOCK
};

enum {
    UI_TEXT_OPT_ORIGIN,
    UI_TEXT_OPT_CENTER,
    UI_TEXT_OPT_VERT_CENTER,
    UI_TEXT_OPT_HORI_CENTER
};

enum {
    UI_MOUSE_LEFT,
    UI_MOUSE_RIGHT,
    UI_MOUSE_FORWARD,
    UI_MOUSE_BACKWARD,
    UI_MOUSE_MIDDLE
};

enum {
    UI_MOUSE_PRESSED = 1,
    UI_MOUSE_RELEASED
};

enum {
    UI_INTERACTION_PRESS = 1,
    UI_INTERACTION_PRESS_AND_RELEASED
};

/* Types */

typedef struct {
    int x, y;
} ui_v2;

typedef struct {
    int x, y, w, h;
} ui_rect;

typedef struct {
    unsigned char r, g, b, a;
} ui_color;

/* Widgets */

typedef struct {
    ui_id ID;
    ui_rect Rect; 

    int ZIndex;

    int IsBeingResized;

    /* TODO: Support multiple columns */
    int RowHeight; 
    int Inline;
    ui_v2 Cursor;
} ui_window;

/* Commands */

typedef struct {
    int ZIndex;
    int CommandCount;
} ui_command_block;

typedef struct {
    ui_rect Rect;
} ui_command_push_clip;

typedef struct {
    ui_rect Rect;
    ui_color Color;
} ui_command_rect;

typedef struct {
    ui_rect Rect;
    ui_color Color;
    char *Text;
} ui_command_text;

typedef struct {
    ui_rect Rect;
    ui_color Color;
    int ID;
} ui_command_icon;

typedef struct {
    int Type;
    int Clip;
    union {
        ui_command_push_clip Clip;
        ui_command_rect Rect;
        ui_command_text Text;
        ui_command_icon Icon;
        ui_command_block Block;
    } Command;    
} ui_command;

typedef struct {
    ui_command *Target; 
    int SortKey;
} ui_command_ref;

typedef struct {
    int TextHeight;
    int (* TextWidth)(char *Text);
    ui_v2 MousePosPrev;
    ui_v2 MousePos;

    ui_id Active;
    ui_id Hot;
    int SomethingIsHot;

    /* The top z-index is incremented each time a window is created as they
     * are created on top, also when a window not on top gets brought to the 
     * top */
    int ZIndexTop;

    int CmdIndex;
    int CmdRefIndex;

    /* If we draw and there's no active block then we're not drawing in a window.
     * In this case set the ZIndex of the block to such that all draw commands not
     * inside a block are drawn first or last */
    ui_command_block *ActiveBlock;

    struct {
        int Active, WasCreatedThisFrame;
        ui_rect Rect;
    } PopUp;

    struct {
        int Active, Button, Type;
        ui_v2 P;
    } MouseEvent;

    unsigned int TextBufferTop;
    char TextBuffer[UI_TEXT_MAX];

    /* The current window being edited between a pair of calls to UI_Window
     * and UI_EndWindow */
    ui_window *WindowSelected;

    struct { unsigned int Index; ui_command_ref Items[UI_COMMAND_MAX]; } CommandRefStack;

    /* Top index is the same as number of windows, i.e. WindowStack.Index.
     * The rationale for WindowDepthOrder is having a place where the order of
     * the windows is described. WindowDepthOrder is not needed for rendering
     * the windows correctly as the block commands include a z-index. But it 
     * does make it easier to determine which window a mouse button press hits. */
    ui_window *WindowDepthOrder[UI_WINDOW_MAX];
    struct { unsigned int Index; ui_window Items[UI_WINDOW_MAX]; } WindowStack;
    struct { unsigned int Index; ui_command Items[UI_COMMAND_MAX]; } CommandStack;
} ui_context;

void UI_Begin(ui_context *Ctx);
void UI_End(ui_context *Ctx);

ui_rect UI_Rect(int x, int y, int w, int h);
ui_color UI_Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
ui_id UI_Hash(char *Name, ui_id Hash);

int UI_NextCommand(ui_context *Ctx, ui_command **Out);

void UI_Window(ui_context *Ctx, char *Name, int x, int y);
ui_window *UI_FindWindow(ui_context *Ctx, ui_id ID);
void UI_EndWindow(ui_context *Ctx);

void UI_MouseButton(ui_context *ctx, int x, int y, int Button, int EventType);
void UI_MousePosition(ui_context *Ctx, int x, int y);

void UI_Text(ui_context *Ctx, char *Text, ui_color Color);
int UI_Button(ui_context *Ctx, char *Label);
void UI_PopUP(ui_context *Ctx);
int UI_Number(ui_context *ctx, float Step, float *Value);
int UI_Slider(ui_context *Ctx, char *Name, float Low, float High, float *Value);

void UI_DrawRectEx(ui_context *Ctx, ui_rect Rect, ui_color Color, int Clip);
void UI_DrawRect(ui_context *Ctx, ui_rect Rect, ui_color Color);
void UI_DrawIcon(ui_context *Ctx, int ID, ui_rect Rect, ui_color Color);
ui_rect UI_DrawTextEx(ui_context *Ctx, char *Text, ui_rect Rect, ui_color Color, int Options, int Free);
ui_rect UI_DrawText(ui_context *Ctx, char *Text, ui_rect Rect, ui_color Color, int Options);
void UI_DrawPopUp(ui_context *Ctx);

void UI_Inline(ui_context *Ctx);

#endif

