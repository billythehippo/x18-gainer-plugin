This is a simple JUCE- and liblo-based plugin to control Behringer X/XR18 gains from your DAW and also keep them in the project file.
The mixer MUST be connected to the same local network with your PC. In this case the plugin will find mixer automatically.

When FEEDBACK is switched on - you can also control the mixer from other PCs and apps with DAW parameter sync;
when off - it will keep gain as DAW parameter when changed from other PCs and apps.

Also if your DAW is closed correctly it will restore gain values that were before the plugin was connected to mixer.

LIMITATIONS (temporary):
1. This plugin now controls only analog gains (USB trims coming soon).
2. This plugin now controls only one mixer per network.

The plugin has been tested as VST3 on Debian GNU/Linux 13 with Ardour and Reaper.

TODO:
1. Add 48V switches
2. Test it on other platforms and other linux distribs;
3. Add support switching GAIN/TRIM;
4. Add support for multiple mixers in the network;
