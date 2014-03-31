@echo off
REM -- First make map file from Microsoft Visual C++ generated resource.h
echo // MAKEHELP.BAT generated Help Map file.  Used by MOD3D.HPJ. >"hlp\Mod3D.hm"
echo. >>"hlp\Mod3D.hm"
echo // Commands (ID_* and IDM_*) >>"hlp\Mod3D.hm"
makehm ID_,HID_,0x10000 IDM_,HIDM_,0x10000 resource.h >>"hlp\Mod3D.hm"
echo. >>"hlp\Mod3D.hm"
echo // Prompts (IDP_*) >>"hlp\Mod3D.hm"
makehm IDP_,HIDP_,0x30000 resource.h >>"hlp\Mod3D.hm"
echo. >>"hlp\Mod3D.hm"
echo // Resources (IDR_*) >>"hlp\Mod3D.hm"
makehm IDR_,HIDR_,0x20000 resource.h >>"hlp\Mod3D.hm"
echo. >>"hlp\Mod3D.hm"
echo // Dialogs (IDD_*) >>"hlp\Mod3D.hm"
makehm IDD_,HIDD_,0x20000 resource.h >>"hlp\Mod3D.hm"
echo. >>"hlp\Mod3D.hm"
echo // Frame Controls (IDW_*) >>"hlp\Mod3D.hm"
makehm IDW_,HIDW_,0x50000 resource.h >>"hlp\Mod3D.hm"
REM -- Make help for Project MOD3D


echo Building Win32 Help files
start /wait hcw /C /E /M "hlp\Mod3D.hpj"
if errorlevel 1 goto :Error
if not exist "hlp\Mod3D.hlp" goto :Error
if not exist "hlp\Mod3D.cnt" goto :Error
echo.
if exist Debug\nul copy "hlp\Mod3D.hlp" Debug
if exist Debug\nul copy "hlp\Mod3D.cnt" Debug
if exist Release\nul copy "hlp\Mod3D.hlp" Release
if exist Release\nul copy "hlp\Mod3D.cnt" Release
echo.
goto :done

:Error
echo hlp\Mod3D.hpj(1) : error: Problem encountered creating help file

:done
echo.
