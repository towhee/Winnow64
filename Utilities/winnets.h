#ifndef WINNETS_H
#define WINNETS_H

#include <QStringList>

/*
    Winnets are small "droplet" executables (built from tools/Winnet) that
    forward image paths to Winnow, acting like Photoshop droplets. They live in
    the per-user Winnets folder:

        Windows: <AppData>/Roaming/Winnow/Winnets
        macOS:   ~/Library/Application Support/Winnow/Winnets

    A master droplet (plus a version-correct shared QtCore on macOS) is staged
    into the Winnow app at build time (see the Winnet target in CMakeLists.txt).
    This module keeps the per-user AppData copies in sync with that master:

        - refreshes the shared Qt runtime (mac: QtCore.framework, win:
          Qt6Core.dll) when the staged one is newer
        - (re)creates a droplet per Embellish template, plus the fixed
          "FocusStack" droplet, copying from the master and refreshing stale
          copies (e.g. after a Winnow/Qt upgrade)
        - removes orphaned Embellish* droplets and obsolete .app bundles

    The renamed droplet forwards its own name as arg[0] (ie "EmbellishZen2048" or
    "FocusStack") followed by the image paths; Winnow's MW::handleStartupArgs
    derives the action and template from arg[0].

    Sync is driven by EmbelProperties when the Embellish template list changes
    (and at startup when the Embellish dock is created), because that is where
    the template list lives.
*/
namespace Winnets {

/*
    embellishDropletNames: the droplet names for the current Embellish templates,
    ie "Embellish" + template name (excluding "Do not Embellish"). The FocusStack
    droplet is always maintained in addition to these.
*/
void sync(const QStringList &embellishDropletNames);

}  // namespace Winnets

#endif  // WINNETS_H
