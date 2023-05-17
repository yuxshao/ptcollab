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
Black=#000000

[fonts]
Editor="Sans serif"
Meter="Sans serif"
```
Notes:
- The alpha channels for colors `FadedWhite` and `Font` in `[parameters]` are discarded.
- The alpha channel in color `Playhead` is halved because the opacity of the playheads vary based on other factors.

The colors can be in any of these 3 formats;
- `#RGB`; "Web colors" -- reduced detail RGB hex value.
- `#RRGGBB`; Normal RGB hex value.
- `#AARRGGBB`; ARGB hex value. 

The best way to get to know these colors is to experiment with them. If you're interested in learning more about these colors and how they behave under the hood, please look at the source; this is the extent of the documentation for the majority of these colors.
