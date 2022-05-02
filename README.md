# RTGI -- Ray Tracing & Global Illumination Framework

## Stand der Dinge

Der Code wird stabiler, aber nach wie vor sind nicht alle Aspekte perfekt durchdacht und die Aufteilung des Codes (insb. entkopplung durch Plugins) weder sehr gut, noch final.

## Verwendung des Repos

Wenn Sie den Code direkt aus dem Repository verwenden sind Sie so zu sagen im Maintainer-Mode, d.h. Sie müssen die nötigen Autotools installiert haben (automake, autoconf, pkg-config). Eventuell auch Shader-Compiler für den Vulkan-Code.

Zur Vorlesung werden Tarballs ausgeteilt in denen Sie nur `configure` ausführen müssen.

Wenn Sie das Repo verwenden müssen Sie das Buildsystem erzeugen und konfigurieren, bevor Sie initial den Code übersetzten können:
```
$ autoreconf -if
$ ./configure
```

Lesen Sie den Output von `configure` bitte sehr genau um zu sehen ob alle Libs die Sie brauchem auf Ihrem System auch gefunden werden.

## Abhängigkeiten

Folgende Pakete sind nötig (Liste für Debianderivate wie z.B. Ubuntu/Mint, weitere nehme ich gerne auf, schicken Sie mir einfach die nötigen Daten!).

Zum Übersetzen des Codes:
```
sudo apt install build-essential pkg-config imagemagick libmagickwand-dev libassimp-dev libglm-dev libpng++-dev
```

Für das Buildsystem wenn Sie das Repo direkt verwenden:
```
sudo apt install autoconf automake autoconf-archive
```

Für Vulkan, OpenGL und Cuda sind weitere Libs/Toolkits nötig, falls Embree verwendet werden soll muss die Lib auch verfügbar sein.
Im Gralab findet sich das alles in /usr/gralab, siehe dazu auch `man gralab`
Welche Libs genau benötig werden kann man ganz gut in `configure.ac` nachlesen.

## Configure & Make

Wenn Sie den Code mit gutem Debug-Support übersetzen wollen (auf Kosten der Laufzeit) können Sie `configure CXXFLAGS="-ggdb3 -O0"` verwenden.
Zum Debuggen empfehle ich `cgdb`.

Übrigens kann mit `make -j N` parallel übersetzt werden, wobei `N` die Anzahl von Jobs ist. Faustregel: etwas mehr als die Anzahl der Prozessorkerne. Auf einem 4-Core System mit Hypterthreading also z.B. `make -j9`, auf meinem 8-Core System `make -j20` (auch wenn es dafür noch nicht genug Sourcefiles gibt).

## Verbesserungen

Wenn Sie eine Verbesserung mit mir teilen möchten können Sie gerne einen Patch senden, noch besser: Forken Sie das Repo, committen Sie den Code und stellen einen Pullrequest :)

## Ausführen

Um das Programm auszuführen verwenden Sie eines der Skriptfiles:
```
./rtgi -s script-sibenik
```
(dazu müssen die entsprechenden Modelle unter `render-data/` liegen, siehe Grips-Seite).

In dem Skript-Fall beendet sich das Programm danach, Sie können es aber auch mit `-l` (für "load") ausführen, dann wird das Skript geladen und ausgeführt, danach können Sie aber noch weitere Kommandos eingeben.
Tipp: Wenn Sie sich in der Szene bewegen wollen drenen Sie `sppx` und `resolution` etwas herunter, dann geht das viel schneller.
Alle verfügbaren Kommandos finden sich in `src/interaction.cpp`, bis auf GI-Algorithmus-spezifische Kommandos, die bei den einzelnen Algorithmen definiert sind (siehe z.B. `src/primary-hit.cpp`).

## Neue Sourcefiles

Bisher ist der Code noch nicht ganz auf die Art und Weise organisiert wie ich das gerne hätte, wenn Sie einen neuen Tracer schreiben, dann legen Sie bitte ein Unterverzeichnis in `rt/` an. Sie können dann das `rt/bbvh-base/Makefile.am` kopieren und anpassen, siehe auch `driver/Makefile.am`. Neue Quelldateien sollten im entsprechenden `Makefile.am` in der Liste hinzugefügt werden, dann sind sie automatisch im Build-Prozess integriert.

## Branching in Projekt/Examensarbeiten

Wenn Sie eine längere Arbeit innerhalb von RTGI machen, erstellen Sie bitte einen FORK des Repositories via Gitlab. In Ihrem Fork, legen Sie dann bitte einen Branch an der also lokaler Masterbranch dient (auch Integrationbranch genannt), Vorschlag <arbeit>-<username>, also z.B. ba-sek38402.

Für einzelne Teilaufgaben können Sie dann, ausgehend von diesem Branch, sogenannte Featurebranches anlegen innerhalb derer Sie die Aufgaben umsetzen können. Damit können wir, wenn Sie Feedback zu Ihrer Umsetzung wollen, einen Pullrequest aufmachen und via Gitlab die Unterschiede zu Ihrem Ursprungsbranch sehen und anhand des Codes kommentieren und entsprechend sehr einfach über Details sprechen.

Wenn wir dann zufrieden mit dem Stand sind können Sie den Featurebranch zurück in Ihren Integrationsbranch mergen.

Sie können regelmäßig vom Original-Repository den master-Branch in Ihren Integrationsbranch mergen, und sollten einzelne Teile oder Ihre Arbeit insgesamt in das Ursprungs-Repository gemerged werden, ist das sehr einfach möglich.

Inspiriert von https://nvie.com/posts/a-successful-git-branching-model/

## Code Conventions

Ein unangenehmes Thema, da aber der Plan ist, dass in das Repo Code aus Projekt- und Examensarbeiten integriert wird würde ich mich sehr freuen wenn Sie Code schreiben der dem Muster des vorhandenen Codes folgt.
Im Zweifel, oder wenn meine Stilbeschreibung unterspezifiziert ist, schauen Sie sich um und orientieren Sie sich an dem, was Sie sehen.

Hier geht es nicht darum Ihnen meine Sicht der Welt aufzudrücken, sondern nachfolgenden Studis eine konsistente Codebasis zur Verfügung zu stellen. Erfahrungsgemäß macht das die Einarbeitung etwas einfacher.

Hier eine erste Sammlung
- Alle Namen (außer globale Konstanten) sind in Kleinbuchstaben und mit Unterstrichen
- Keine Präfixe oder Suffixe (z.B. `m_member`, `member_`, `imy_interface`)
- Das gilt auch für get/set, C++ Standard ist eher `int param() const /*getter*/; void param(int) /*setter*/;`
- Wenn ein Member eh getter und setter hat, dann kann es auch public sein
- Ifs und Schleifen sollten nur dann {}-Klammern haben, wenn es nötig ist
- { auf der Zeile des Ifs oder der Schleife
- } allein in einer Zeile (nicht `} else {`)
- Formatierung gilt auch für Klassen und Funktionen außer es wird wirklich unübersichtlich
- Konstruktor-Initialisierer in der Signatur-Zeile oder direkt darunter, in dem Fall `:` eingerückt und die `,` am Ende der Zeile (siehe vorhandene Beispiele)
- Keine unnötigen Kommentare (`// destructor`)
- Bitte keine standard "Continuation Lines", sondern semantisch einrücken, also alle Funktionsargumente ab der Zeile nach der entsprechenden öffnenden Klammer, ebenso bei Operator-Ketten, sinnvoll umbrechen und beim ersten Operator auf der vorigen Zeile anfangen. Also:
```
    foobar(an_argument, another_argument_with_a_long_name,
           [] (int x) { 
               return x;
           },
           more_ars);
```
- Wichtige Bitte: den Code nicht automatisch formatieren lassen, da geht immer etwas verloren, insb. z.B. korrekte Einrückung für "Alignment" (also: Tabs für Nesting, Spaces für Ausrichtung z.B. von Continuation Lines, dann sieht die ganze Sache mit anderen Tab-Settings auch noch ok aus).

Das ist bestimmt nicht vollständig und im Zweifel gilt natürlich immer "hauptsache lesbar und klar", aber ich würde darum bitten, dass Sie versuchen sich daran zu orientieren :)

Eine weitere Bitte falls Sie keinen ausgeprägten C++ Hintergrund haben: Schauen Sie sich ein Tutorial für Newcomer von Ihrer "Heimatsprache" an, so dass Sie Code schreiben der "üblich" aussieht.

## BVH Export

Die verwendeten AABBs (bisher nur für Binary BVHs) können mit dem Kommando `bvh export TIEFE DATEINAME.obj` als OBJ-Datei exportiert, und in Blender zur Veranschaulichung importiert werden.
Hierbei muss das export Kommando nach dem `commit` stehen. Wenn beim Import in Blender die Einstellungen `Split by Group` gesetzt wird, ist es möglich die verschiedenen Tiefen/Level der BVH ein- und auszublenden. Mit `Z` kann in Blender zwischen Solid und Wireframe Ansicht gewechselt werden.
