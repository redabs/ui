# ui
![image](https://i.imgur.com/FtRtZjY.png)

## Rendering
The user draws the UI themselves. To get the draw commands for rendering the user calls `UI_NextCommand`.
```c
int UI_NextCommand(ui_context *Ctx, ui_command **Command);
```
`UI_NextCommand` returns 1 if a pointer to a command was written to `Command` or 0 if there are no more commands.

Current command types are:
* UI_COMMAND_PUSH_CLIP: Defines a clip rectangle.
* UI_COMMAND_TEXT: Defines a color, a string of characters and a rectangle in which the text is rendered.
* UI_COMMAND_ICON: Defines a destination rectangle, a color and the ID of a icon to rendered.
* UI_COMMAND_RECT: Defines a solid color rectangle to be rendred.
* UI_COMMAND_POP_CLIP: Hints to user to pop the most recently pushed clip rectangle.

