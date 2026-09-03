# NDS Loc Utils

WIP. Custom tool to help with Nintendo DS roms localization. Will be available as a GUI tool and bespoke library.

This tool is primarily intended to be used with the Kingdom Hearts games for the NDS, but more games could be handled later.

## Features
- reverse-engineering of the proprietary P2, CAKP, and string formats from KH DS
- extraction of all the strings in the rom dump to a .csv file
- all languages are handled (one .csv per language)
- modify the .csv, and then you can re-export the .nds rom with the updated lines

## TODO
- capacity overflow is not allowed: if a 5-char string should be replaced with a 7-char string, it gets discarded.
- add bespoke "lib" project in CMake (to include in other projects without dependency to Qt)
- some string formats haven't been found yet
