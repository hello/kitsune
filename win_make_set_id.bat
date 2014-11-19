echo off
if exist %~dp0kitsune\kitsune_version.h goto have_file

echo "No kitsune version, making default -1"
@echo #ifndef KIT_VER > %~dp0kitsune\kitsune_version.h
@echo #define KIT_VER -1 >> %~dp0kitsune\kitsune_version.h
@echo #endif >> %~dp0kitsune\kitsune_version.h
goto end

:have_file
echo "You already have a kitsune version file!"

:end