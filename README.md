This is a simple JUCE8- and tinyOSC-based plugin to control Behringer X/XR18 gains, links and phantoms
from your DAW and also keep them in the project file. The mixer MUST be connected to the same local network with your PC.
In this case the plugin will find mixer automatically.

When FEEDBACK is switched on - you can also control the mixer from other PCs and apps with DAW parameter sync;
when off - it will keep gains, links and phantoms as DAW parameter when changed from other PCs and apps.

Also if your DAW is closed correctly it will restore gains, phantoms and links that were before the plugin was connected to mixer.

LIMITATIONS (temporary):
1. This plugin now controls only analog gains (USB trims coming soon).
2. This plugin now controls only one mixer per network.

The plugin has been tested as:
1. Debian GNU/Linux 13: Ardour and Reaper, VST3 and LV2;
2. Manjaro Linux: Ardour and Reaper, VST3 and LV2;
3. Mac OS Sequoia: Reaper, VST3, AU and LV2

Building:
1. Open .jucer file with Projucer, add exporters you want, save project;
2. LINUX: just open treminal in <PROJECT>/Build/LinuxMakefile and type "make":
   it will copy plugins to ~/.vst3 and ./lv2 after building.
   MacOS: use XCode or 

TODO:
1. Test it on Windows and some linux distribs;
2. Add support for USB TRIMs;
3. Multiple mixers in the network version;
