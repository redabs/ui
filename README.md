# ui
This project is very _not_ finished but has some basic functionality.
![image](https://i.imgur.com/w0wNWQX.png)

## Usage
The library context exposes a command stack for rendering.
The user has to supply one callback function to the library.
```c
int (* TextWidth)(char *Text);
```

Current command types are:
* ui_command_push_clip: Clip rectangle
* ui_command_text: This command defines a rectangle in which some text is rendered. 
* ui_command_icon: Defines a destionation rectangle a color and the ID of a icon to rendered.
* ui_command_rect: Defines a solid color rectangle to be rendred.
* UI_COMMAND_POP_CLIP: A single enum which tells the user to pop a clip rect from the clip rect stack. 

