# Microsoft Developer Studio Project File - Name="Mod3D" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=MOD3D - WIN32 DEBUG
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Mod3D.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Mod3D.mak" CFG="MOD3D - WIN32 DEBUG"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Mod3D - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "Mod3D - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Mod3D - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /FR /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x407 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x407 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "Mod3D - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x407 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x407 /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "Mod3D - Win32 Release"
# Name "Mod3D - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\3DView.cpp
# End Source File
# Begin Source File

SOURCE=.\Body.cpp
# End Source File
# Begin Source File

SOURCE=.\BodyPropSheet.cpp
# End Source File
# Begin Source File

SOURCE=.\ChildFrm.cpp
# End Source File
# Begin Source File

SOURCE=.\Contour.cpp
# End Source File
# Begin Source File

SOURCE=.\DlgBodyExport.cpp
# End Source File
# Begin Source File

SOURCE=.\DlgBodyImport.cpp
# End Source File
# Begin Source File

SOURCE=.\DlgBodyModf.cpp
# End Source File
# Begin Source File

SOURCE=.\DlgCompProp.cpp
# End Source File
# Begin Source File

SOURCE=.\DlgDefineObs.cpp
# End Source File
# Begin Source File

SOURCE=.\DlgEditBodies.cpp
# End Source File
# Begin Source File

SOURCE=.\DlgExportField.cpp
# End Source File
# Begin Source File

SOURCE=.\DlgImportField.cpp
# End Source File
# Begin Source File

SOURCE=.\DlgInducingField.cpp
# End Source File
# Begin Source File

SOURCE=.\DlgInsExistBoby.cpp
# End Source File
# Begin Source File

SOURCE=.\DlgpBodyDescription.cpp
# End Source File
# Begin Source File

SOURCE=.\DlgpBodyDrw.cpp
# End Source File
# Begin Source File

SOURCE=.\DlgpBodyPhys.cpp
# End Source File
# Begin Source File

SOURCE=.\DlgSelectBody.cpp
# End Source File
# Begin Source File

SOURCE=.\DlgVerticalRange.cpp
# End Source File
# Begin Source File

SOURCE=.\Facet3Pt.cpp
# End Source File
# Begin Source File

SOURCE=.\Geology.cpp
# End Source File
# Begin Source File

SOURCE=.\GeolUnit.cpp
# End Source File
# Begin Source File

SOURCE=.\GraphAxis.cpp
# End Source File
# Begin Source File

SOURCE=.\GraphAxisX.cpp
# End Source File
# Begin Source File

SOURCE=.\GraphAxisY.cpp
# End Source File
# Begin Source File

SOURCE=.\GrdProfView.cpp
# End Source File
# Begin Source File

SOURCE=.\Grid.cpp
# End Source File
# Begin Source File

SOURCE=.\MainFrm.cpp
# End Source File
# Begin Source File

SOURCE=.\Mod3D.cpp
# End Source File
# Begin Source File

SOURCE=.\hlp\Mod3D.hpj

!IF  "$(CFG)" == "Mod3D - Win32 Release"

# PROP Ignore_Default_Tool 1
USERDEP__MOD3D="hlp\AfxCore.rtf"	"hlp\AfxPrint.rtf"	"hlp\$(TargetName).hm"	
# Begin Custom Build - Making help file...
OutDir=.\Release
TargetName=Mod3D
InputPath=.\hlp\Mod3D.hpj
InputName=Mod3D

"$(OutDir)\$(InputName).hlp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	start /wait hcw /C /E /M "hlp\$(InputName).hpj" 
	if errorlevel 1 goto :Error 
	if not exist "hlp\$(InputName).hlp" goto :Error 
	copy "hlp\$(InputName).hlp" $(OutDir) 
	goto :done 
	:Error 
	echo hlp\$(InputName).hpj(1) : error: 
	type "hlp\$(InputName).log" 
	:done 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "Mod3D - Win32 Debug"

# PROP Ignore_Default_Tool 1
USERDEP__MOD3D="hlp\AfxCore.rtf"	"hlp\AfxPrint.rtf"	"hlp\$(TargetName).hm"	
# Begin Custom Build - Making help file...
OutDir=.\Debug
TargetName=Mod3D
InputPath=.\hlp\Mod3D.hpj
InputName=Mod3D

"$(OutDir)\$(InputName).hlp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	start /wait hcw /C /E /M "hlp\$(InputName).hpj" 
	if errorlevel 1 goto :Error 
	if not exist "hlp\$(InputName).hlp" goto :Error 
	copy "hlp\$(InputName).hlp" $(OutDir) 
	goto :done 
	:Error 
	echo hlp\$(InputName).hpj(1) : error: 
	type "hlp\$(InputName).log" 
	:done 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Mod3D.rc
# End Source File
# Begin Source File

SOURCE=.\Mod3DDoc.cpp
# End Source File
# Begin Source File

SOURCE=.\Mod3DView.cpp
# End Source File
# Begin Source File

SOURCE=.\Point3D.cpp
# End Source File
# Begin Source File

SOURCE=.\PotField.cpp

!IF  "$(CFG)" == "Mod3D - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Mod3D - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\ScaleBar.cpp
# End Source File
# Begin Source File

SOURCE=.\Shape.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\3DView.h
# End Source File
# Begin Source File

SOURCE=.\Body.h
# End Source File
# Begin Source File

SOURCE=.\BodyPropSheet.h
# End Source File
# Begin Source File

SOURCE=.\ChildFrm.h
# End Source File
# Begin Source File

SOURCE=.\Contour.h
# End Source File
# Begin Source File

SOURCE=.\DlgBodyExport.h
# End Source File
# Begin Source File

SOURCE=.\DlgBodyImport.h
# End Source File
# Begin Source File

SOURCE=.\DlgBodyModf.h
# End Source File
# Begin Source File

SOURCE=.\DlgCompProp.h
# End Source File
# Begin Source File

SOURCE=.\DlgDefineObs.h
# End Source File
# Begin Source File

SOURCE=.\DlgEditBodies.h
# End Source File
# Begin Source File

SOURCE=.\DlgExportField.h
# End Source File
# Begin Source File

SOURCE=.\DlgImportField.h
# End Source File
# Begin Source File

SOURCE=.\DlgInducingField.h
# End Source File
# Begin Source File

SOURCE=.\DlgInsExistBoby.h
# End Source File
# Begin Source File

SOURCE=.\DlgpBodyDescription.h
# End Source File
# Begin Source File

SOURCE=.\DlgpBodyDrw.h
# End Source File
# Begin Source File

SOURCE=.\DlgpBodyPhys.h
# End Source File
# Begin Source File

SOURCE=.\DlgSelectBody.h
# End Source File
# Begin Source File

SOURCE=.\DlgVerticalRange.h
# End Source File
# Begin Source File

SOURCE=.\Facet3Pt.h
# End Source File
# Begin Source File

SOURCE=.\Geology.h
# End Source File
# Begin Source File

SOURCE=.\GeolUnit.h
# End Source File
# Begin Source File

SOURCE=.\GraphAxis.h
# End Source File
# Begin Source File

SOURCE=.\GraphAxisX.h
# End Source File
# Begin Source File

SOURCE=.\GraphAxisY.h
# End Source File
# Begin Source File

SOURCE=.\GrdProfView.h
# End Source File
# Begin Source File

SOURCE=.\Grid.h
# End Source File
# Begin Source File

SOURCE=.\MainFrm.h
# End Source File
# Begin Source File

SOURCE=.\Messages.h
# End Source File
# Begin Source File

SOURCE=.\Mod3D.h
# End Source File
# Begin Source File

SOURCE=.\Mod3DDoc.h
# End Source File
# Begin Source File

SOURCE=.\Mod3DView.h
# End Source File
# Begin Source File

SOURCE=.\Point3D.h
# End Source File
# Begin Source File

SOURCE=.\PotField.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h

!IF  "$(CFG)" == "Mod3D - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Making help include file...
TargetName=Mod3D
InputPath=.\Resource.h

"hlp\$(TargetName).hm" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	echo. >"hlp\$(TargetName).hm" 
	echo // Commands (ID_* and IDM_*) >>"hlp\$(TargetName).hm" 
	makehm ID_,HID_,0x10000 IDM_,HIDM_,0x10000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Prompts (IDP_*) >>"hlp\$(TargetName).hm" 
	makehm IDP_,HIDP_,0x30000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Resources (IDR_*) >>"hlp\$(TargetName).hm" 
	makehm IDR_,HIDR_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Dialogs (IDD_*) >>"hlp\$(TargetName).hm" 
	makehm IDD_,HIDD_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Frame Controls (IDW_*) >>"hlp\$(TargetName).hm" 
	makehm IDW_,HIDW_,0x50000 resource.h >>"hlp\$(TargetName).hm" 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "Mod3D - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Making help include file...
TargetName=Mod3D
InputPath=.\Resource.h

"hlp\$(TargetName).hm" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	echo. >"hlp\$(TargetName).hm" 
	echo // Commands (ID_* and IDM_*) >>"hlp\$(TargetName).hm" 
	makehm ID_,HID_,0x10000 IDM_,HIDM_,0x10000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Prompts (IDP_*) >>"hlp\$(TargetName).hm" 
	makehm IDP_,HIDP_,0x30000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Resources (IDR_*) >>"hlp\$(TargetName).hm" 
	makehm IDR_,HIDR_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Dialogs (IDD_*) >>"hlp\$(TargetName).hm" 
	makehm IDD_,HIDD_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Frame Controls (IDW_*) >>"hlp\$(TargetName).hm" 
	makehm IDW_,HIDW_,0x50000 resource.h >>"hlp\$(TargetName).hm" 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\ScaleBar.h
# End Source File
# Begin Source File

SOURCE=.\Shape.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\VersionType.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\b1.bmp
# End Source File
# Begin Source File

SOURCE=.\res\b2.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00001.bmp
# End Source File
# Begin Source File

SOURCE=.\res\cell.cur
# End Source File
# Begin Source File

SOURCE=.\res\cur00001.cur
# End Source File
# Begin Source File

SOURCE=.\res\cur00002.cur
# End Source File
# Begin Source File

SOURCE=.\res\cursor_e.cur
# End Source File
# Begin Source File

SOURCE=.\res\cursor_h.cur
# End Source File
# Begin Source File

SOURCE=.\res\cursor_v.cur
# End Source File
# Begin Source File

SOURCE=.\res\cursor_z.cur
# End Source File
# Begin Source File

SOURCE=.\res\cursorzo.cur
# End Source File
# Begin Source File

SOURCE=.\res\DlgBk.bmp
# End Source File
# Begin Source File

SOURCE=.\res\DlgBk_BW.bmp
# End Source File
# Begin Source File

SOURCE=.\res\HScBarAct.bmp
# End Source File
# Begin Source File

SOURCE=.\res\HScBarPas.bmp
# End Source File
# Begin Source File

SOURCE=.\res\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\res\idc_vert.ico
# End Source File
# Begin Source File

SOURCE=.\res\Mod3D.ico
# End Source File
# Begin Source File

SOURCE=.\res\Mod3D.rc2
# End Source File
# Begin Source File

SOURCE=.\res\Mod3DDoc.ico
# End Source File
# Begin Source File

SOURCE=.\res\modPrpo.bmp
# End Source File
# Begin Source File

SOURCE=.\res\TbBmp.bmp
# End Source File
# Begin Source File

SOURCE=.\res\TlbFld.bmp
# End Source File
# Begin Source File

SOURCE=.\res\TlbFldSc.bmp
# End Source File
# Begin Source File

SOURCE=.\res\TlbFldView.bmp
# End Source File
# Begin Source File

SOURCE=.\res\TlbPrfView.bmp
# End Source File
# Begin Source File

SOURCE=.\res\TlbProfView.bmp
# End Source File
# Begin Source File

SOURCE=.\res\Toolbar.bmp
# End Source File
# Begin Source File

SOURCE=.\res\toolbar1.bmp
# End Source File
# Begin Source File

SOURCE=.\res\toolbar_.bmp
# End Source File
# Begin Source File

SOURCE=.\res\ToolBarFldCnt.bmp
# End Source File
# End Group
# Begin Group "Help Files"

# PROP Default_Filter "cnt;rtf"
# Begin Source File

SOURCE=.\hlp\AfxCore.rtf
# End Source File
# Begin Source File

SOURCE=.\hlp\AfxPrint.rtf
# End Source File
# Begin Source File

SOURCE=.\hlp\AppExit.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\Bullet.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\CurArw2.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\CurArw4.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\CurHelp.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\EditCopy.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\EditCut.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\EditPast.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\EditUndo.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\FileNew.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\FileOpen.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\FilePrnt.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\FileSave.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\HlpSBar.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\HlpTBar.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\Mod3D.cnt

!IF  "$(CFG)" == "Mod3D - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Copying contents file...
OutDir=.\Release
InputPath=.\hlp\Mod3D.cnt
InputName=Mod3D

"$(OutDir)\$(InputName).cnt" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy "hlp\$(InputName).cnt" $(OutDir)

# End Custom Build

!ELSEIF  "$(CFG)" == "Mod3D - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Copying contents file...
OutDir=.\Debug
InputPath=.\hlp\Mod3D.cnt
InputName=Mod3D

"$(OutDir)\$(InputName).cnt" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy "hlp\$(InputName).cnt" $(OutDir)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\hlp\RecFirst.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\RecLast.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\RecNext.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\RecPrev.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\Scmax.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\ScMenu.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\Scmin.bmp
# End Source File
# End Group
# Begin Source File

SOURCE=.\Mod3D.reg
# End Source File
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# End Target
# End Project
