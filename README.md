# pxtone collab
<div align="center"> <img src="screenshot.png" alt="ptcollab" style="display: block" /> </div>
<div align="center">pxtone collab is a music editor where you can collaborate with friends!</div>

## Installation
If you're building from source, it should work with a Qt5.15 install, but does
require ogg/vorbis and C++17.

On Mac, you may need to install a later version of clang (e.g., through `brew
install llvm`).

Windows build included in releases page. 

## Features
Most features in pxtone collage are available here:
- A piano roll and parameter editor
- Ctrl+Z and Ctrl+Y/Ctrl+Shift+Z to undo and redo
- Setting tempo, beats, repeat and last measures
- Adding and removing voices and units
- Shift+click to seek
- Ctrl+click to modify just note values, not note ons
- Selection
  - Ctrl+shift+click to select
  - Ctrl+A to select all
  - Ctrl+shift+rclick or Ctrl+D to deselect
- Ctrl+C and Ctrl+V to copy/paste, with ways to configure what
  to copy/paste
- A parameter bar

Additionally, there are a few features I wanted to have
available to make editing easy (for me at least). These are:
1. Editing notes while playing
2. Colour-coded units visible in the background
3. Transposition / shifting parameters by pressing (shift)
   up / down while something is selected. Ctrl shifts by an
   octave.
4. Keyboard shortcuts for navigation
   - W/S for next previous / next unit (PgUp / PgDn also work)
   - Q/A for previous / next parameter
   - F1..F4 for the tabs in the menu
   - 1..9 for jumping to a unit directly
5. Mouse -related shortcuts for view / mode changes
   - Middle -click drag to move around
   - Shift+wheel to scroll horizontally
   - Ctrl+(shift)+wheel to zoom vertically (horizontally)
   - Alt+wheel to change quantization
6. Various dragging gestures
   - Drag in the keyboard view to modify velocity
   - Drag in the parameter view for linear editing

There are also a few (experimental) collaboration-related treats:
1. Watching and following other users' cursors in a shared session
2. Recording a session (via "history_save_file" in the settings file) and
   playing it back later

## Thanks
Again, huge thanks to everyone who helped support this! pxtone
collage is a wonderful music tool and this is I hope is seen as
a kind of tribute to the artist community that's flourished
around it. Here's a tentative running thank-you list - please
let me know if I forgot anyone:
1. Pixel for making such an awesome music software!
2. Everyone involved in ptweb!
3. Friends and family for supporting this!
4. jaxcheese, neozoid, steedfarmer, ewangreen, gkpotter for testing out
   with me, making tunes together, and giving great feedback!
5. steedfarmer for helping me hunt down this nasty networking bug!
6. arcofdream for helping conceptualize the UI, giving feedback,
   and testing it out!
7. arcofdream, nanoplink, easynam for general programming, UX
   discussion and consulting!
8. Jade for testing, finding UI bugs, and providing valuable
   feedback and suggestions!
9. gkpotter and arcofdream for helping getting this ready for release!
10. steedfarmer for figuring out the mac build, and gkpotter for providing the
    deployment!
11. Everyone in the pxtone discord servers!
