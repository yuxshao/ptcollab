### config.ini
This is an example of a complete config.ini file.

```ini
[palette]
Window=#4E4B61
WindowText=#D2CA9C
Base=#4E4B61
ToolTipBase=#4E4B61
ToolTipText=#D2CA9C
Text=#D2CA9C
Button=#4E4B61
ButtonText=#D2CA9C
BrightText=#ff0000
Link=#9D9784
Highlight=#9D9784
HighlightedText=#00003E
Light=#5E5A74
Dark=#413E51

[meter]
Background=#1A1949
BackgroundSoft=#1E1A1949
Bar=#00F080
BarMid=#FFFF80
Label=#D2CA9C
Tick=#343241
BarHigh=#FF0000

[views]
Playhead=#FFFFFF
PlayheadRecording=#FF6666
Cursor=#FFFFFF

[measure]
MeasureSeparator=#FFFFFF
MeasureIncluded=#800000
MeasureExcluded=#400000 
Beat=#808080
UnitEdit=#400070
MeasureNumberBlock=#606060

[parameters]
Blue=#343255
DarkBlue=#1A1949
DarkTeal=#006060
BrightGreen=#00F080
FadedWhite=#FFFFFF
Font=#FFFFFF
Beat=#808080
Measure=#FFFFFF

[keyboard]
Beat=#808080
Measure=#FFFFFF
RootNote=#544C4C
WhiteNote=#404040
BlackNote=#202020
WhiteLeft=#80837E78
BlackLeft=#804E4B61
BlackLeftInner=#1A1949
Black=#000000

[platform]
WindowBorder="#494949"
WindowText="#000000"
WindowCaption="#FFFFFF"
WindowDark=false

[fonts]
Editor="Sans serif"
Meter="Sans serif"
```

Notes:
- The alpha channels for colors `FadedWhite` and `Font` in `[parameters]` are discarded.
- The alpha channel in color `Playhead` is halved because the opacity of the playheads vary based on other factors.
- All of the settings in the `platform` section have platform-dependent behavior. All of the color values will have their alpha ignored.
  - On Windows 10, the only setting that works is `WindowDark`, which influences whether the title bar uses the operating system's immersive dark mode.
  - On Windows 11, `WindowCaption`, `WindowBorder`, and `WindowText` correspond to the title bar's draggable area, borders, and window title respectively. `WindowDark` does the same thing as it does on Windows 10 if `WindowCaption` has not been set.
  - On macOS, `WindowCaption` loosely influences the color of the title bar. It is not exact because the operating system manipulates the color to ensure there can be enough contrast between it and the title bar text. This means that sometimes the color will be lighter or darker than what you put in.
  - On other operating systems, these parameters do nothing.

The colors can be in any of these 3 formats;
- `#RGB`; "Web colors" -- reduced detail RGB hex value.
- `#RRGGBB`; Normal RGB hex value.
- `#AARRGGBB`; ARGB hex value. 

The best way to get to know these colors is to experiment with them. If you're interested in learning more about these colors and how they behave under the hood, please look at the source; this is the extent of the documentation for the majority of these colors.
