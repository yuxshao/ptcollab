#### This file is best read [online](https://github.com/yuxshao/ptcollab/blob/master/src/styles/README.md).

# Styles in ptcollab
You can add more styles by putting them in this directory. For a style to appear in the editor, they must use the following format: 

`*/styles/StyleName/StyleName.qss`

The asterisk represents a number of different directories that ptcollab looks in for styles. Those directories are as follows.
- Directly next to the executable (`./styles/`).
- In a sibling directory called "share" (`../share/styles/`).
- Every location [here (QStandardPaths::AppLocalDataLocation)](https://doc.qt.io/qt-5/qstandardpaths.html#StandardLocation-enum), followed by `/styles/`. In practice, this is usually one of a few directories. These directories can vary wildly on your platform and the way you got ptcollab. Usually, they are as follows:
	- Windows: `C:/Users/<USER>/AppData/Local/ptcollab/styles/`.
	- MacOS: `~/Library/Application Support/ptcollab/styles/`.
	- Linux: `~/.local/share/ptcollab/ptcollab/styles/`.

It's more likely, though, that you're reading this file because you pressed a button in the settings menu of ptcollab. This means that you're already in the right place to start creating or importing styles.

## Importing Styles

Styles can be dropped directly into this folder.

1. Copy your style directly into this folder.
2. Open ptcollab and see if your style is present.
3. If not, read the rest of this document...

## Creating Styles

ptcollab styles are created using Qt Stylesheets. Any custom objects you would want to style can be found in the [source code](https://github.com/yuxshao/ptcollab). Everything else (the majority) that can be styled should be referred to by the [Qt Style Sheets Reference](https://doc.qt.io/qt-5/stylesheet-reference.html).

This is the bare minimum you need for a functioning style:
- `StyleName/StyleName.qss`
- `StyleName/palette.ini`

The name of the containing directory & the name of the stylesheet file (`.qss`) have to match.

For the stylesheet:
- Qt Stylesheets have severe limitations compared to those of the W3C CSS specification (what you're probably used to using CSS with). They do not support variables, animations, or anything else particularly fancy. Below are some helpful QSS resources.
	- [The Style Sheet Syntax](https://doc.qt.io/qt-5/stylesheet-syntax.html)
	- [Qt Style Sheets Reference](https://doc.qt.io/qt-5/stylesheet-reference.html)
- You can use image resources via. `url()`, but files must be referred to relative to the root of the style folder, not the location of the executable.
	- `background-image: url(image.png)` refers to `StyleName/image.png`.

For the palette:

```ini
[palette]
Window=COLOR
WindowText=COLOR
Base=COLOR
ToolTipBase=COLOR
ToolTipText=COLOR
Text=COLOR
Button=COLOR
ButtonText=COLOR
BrightText=COLOR
Link=COLOR
Highlight=COLOR
Light=COLOR
Dark=COLOR
``` 

Wherever you see `COLOR` must be occupied by a valid [QColor](https://doc.qt.io/qt-5/qcolor.html#name) value, converted from a string. In practice, this is best represented in the RGB hex color code you're probably familiar with (`#FFFFFF` is white). The values to be set are those of a [QPalette](https://doc.qt.io/qt-5/qpalette.html#details).

### Basic Debugging

If your stylesheet contains errors, ptcollab will not be able to detect them (it is an issue with Qt -- the topic is brought up in more detail [here](https://forum.qt.io/topic/130386/qcss-parser-how-to-reproduce-behavior)). Depending on the scope of the errors, a few things will happen making it apparent what the issue is.

- No style applied:
  This means that the stylesheet has a top-level error (usually an error in a selector list or a stray character in the topmost level of the stylesheet -- outside of all brackets). For example:
```css
QWidget {
	 background-color: blue;
}
QPushButton {
	 background-color: green;
	 /* The error is here -- no closing brace. */
``` 
- Incomplete styling of a particular object:
  The stylesheet for that object could not be parsed properly, and it is an error in the code within any brackets for those selectors. Check that you didn't miss parenthesis or a semicolon. For example:
```css
QWidget {
  background-color: blue /* The error is here -- no semicolon. */
  border: 2px solid;
}
```
- Everything that hasn't been explicitly styled is black:
  `palette.ini` is absent or unreadable.


## Other

Thanks to Ewan Green for providing the style-switching mechanism as well as creating a style that mimics ptCollage's.
