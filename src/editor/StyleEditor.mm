#if defined(Q_OS_MACOS)
#include "StyleEditor.h"
#import <Cocoa/Cocoa.h>

void setMacOsTitleBar(WId w) {
    QColor caption = StyleEditor::config.color.WindowCaption;
    if(caption.isValid()) {
        auto qColorToNsColor = [] (const QColor &c) -> NSColor * {
            qDebug() << c.redF() << c.greenF() << c.blueF();
            return [NSColor
                    colorWithCalibratedRed:c.redF()
                    green:c.greenF()
                    blue:c.blueF()
                    alpha:1.0];
        };

        NSWindow *win = [reinterpret_cast<NSView*>(w) window];
        win.titlebarAppearsTransparent = true;
        win.backgroundColor = qColorToNsColor(caption);

        /* Unlike on Windows, macOS (Ventura at least) does some "intelligent"
           stuff with the color you give it, in order to make sure contrast
           is sufficient between the window text (the color of which you can't
           change) when it's in its different states (active, disabled, whatever).
           MEANING, this color is really more of a suggestion than anything
           else & nothing is guaranteed; you may still have a seam between your
           title bar & the app's space. I think that's fine; if your style is
           blue, and you set WindowCaption to a blue color, it will still be
           blue & your goal achieved (even if not exactly how you wanted it).
           - Ewan
        */
    }
}
#endif
