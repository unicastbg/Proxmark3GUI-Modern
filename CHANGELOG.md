# Change Log

This project is a modernized Windows-focused fork of
[wh201906/Proxmark3GUI](https://github.com/wh201906/Proxmark3GUI). Entries
from `V0.2.8` and earlier are inherited from the original project history.

## Unreleased

- Documented direct Windows RRG/Iceman client download links from ProxmarkBuilds.
- Documented the official RRG/Iceman project links for users who want to inspect
  or build the Proxmark client themselves.

## v0.3.0 - 2026-08-25

- Added a modern Simple page for common scan, identify, clone, and verify flows.
- Added a visual Proxmark3 placement guide with RF/NFC and LF pad states.
- Added guided dump/restore workflow for supported MIFARE Classic cards.
- Added card data map display for readable dump content.
- Added support for the modern RRG/Iceman v4.21611 command configuration.
- Improved COM port discovery, including later USB plug-in detection.
- Improved connection state handling so Stop, Connect, and Disconnect behave
  predictably.
- Added firmware/client detection messaging for Iceman/RRG clients.
- Added cleanup support for generated Proxmark dump/key files.
- Reworked the top navigation into Simple, Advanced, and Settings sections.
- Removed legacy opacity/language controls that no longer fit the modern UI.
- Added a bundled dark theme and application icon.
- Added screenshots and a Windows-focused README.
- Added a Modern UI NSIS installer that defaults to Program Files.
- Published the first GitHub release with a Windows installer asset.

## Original Project History

[Original Chinese changelog](doc/CHANGELOG/CHANGELOG_zh_CN.md)  

### V0.2.8
+ Add support for Iceman/RRG repo v4.16717  
+ Fix some bugs  
+ Make it easier for testing this GUI across multiple clients  
+ Add support for Bluetooth and TCP connection  

### V0.2.7
+ Fix writing to Block 0 failure when using with RRG repo v4.15864  
+ Disable disconnection detection on Linux/macOS by default  
+ Fix a little bug in the config file  
+ Fix the Trailer Decoder  
+ Show more details in the Trailer Decoder  
+ Add dark theme(from https://github.com/ColinDuquesnoy/QDarkStyleSheet)  
+ Add support for customizable theme, opacity and fonts  
+ Fix translations  

### V0.2.6
+ Add support for Iceman/RRG repo v4.15864 [#37](https://github.com/wh201906/Proxmark3GUI/issues/37)  
+ Optimize mifare classic block writing logic  
+ Fix the default lf config  
+ Add feedback for the GUI failing to start the client  
+ Add feedback for the client failing to connect to PM3 hardware  
+ Detect PM3 hardware when searching serial ports  
+ Remove extra empty lines in raw command output  
+ Use embedded config files  
+ Remove the wait time between performing nested attack then switching to staticnested attack  

### V0.2.5
+ Fix bug [#28](https://github.com/wh201906/Proxmark3GUI/issues/28)  

### V0.2.4
+ Clone EM410x card to T55xx card  

### V0.2.3
+ Fix bug [#27](https://github.com/wh201906/Proxmark3GUI/issues/27)  
+ Try to support Non-ASCII path  

### V0.2.2
+ Load command format from external json file  
+ Fix bug [#20](https://github.com/wh201906/Proxmark3GUI/issues/20), [#21](https://github.com/wh201906/Proxmark3GUI/issues/21), [#22](https://github.com/wh201906/Proxmark3GUI/issues/22)  
+ Support Iceman/RRG repo v4.13441

### V0.2.1
+ Optimize MIFARE Classic reading logic  
+ Fix bug [#16](https://github.com/wh201906/Proxmark3GUI/issues/16)  
+ Fix bug [#15](https://github.com/wh201906/Proxmark3GUI/issues/15) partially (the path can contain spaces now)  

### V0.2
+ Use Dock widget for more flexible layout  
+ Support basic LF commands  
+ Fix a bug in RawCommand tab  

### V0.1.4
+ Optimize performance  
+ Optimize UI  
+ Search available ports automatically  
+ Add High-DPI support  
+ Support configuring environment variables by script  
(Useful when the client requires specific environment variables)  
+ All functions are compatible with Iceman/RRG repo(tested on v4.9237)  
+ Support specifying client working directory
+ Fix some bugs

### V0.1.3
+ Fix Trailer Decoder
+ Add feedback when writing selected blocks

### V0.1.2
+ Optimize read logic
+ Make UI Customizable
+ Save client path automatically
+ Add Trailer Decoder(Deprecated, plz use V0.1.3 or higher version)
+ Support read/write selected blocks
+ Support a few Iceman functions
+ Fix some bugs

### V0.1.1
+ Complete Mifare module(support simulate and sniff)

### V0.1
+ Able to deal with Mifare card and related files

### V0.0.1
+ a dumb version with a useless GUI and a serial choose box.
