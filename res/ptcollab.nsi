; NSIS installer script for ptcollab based on Modern UI 2
; Written by Ewan Green, based off example script by Joost Verburg

!ifndef VERSION
	!error "Version not defined at time of compilation. Pass /DVERSION= to makensis."
!endif
!ifndef ARCH
	!error "Architecture not defined at time of compilation. Pass /DARCH= to makensis."
!endif
!ifndef IN_PATH
	!error "No source path provided."
!endif

!if "${ARCH}" == "amd64"
	InstallDir "$PROGRAMFILES64\ptcollab"
	!define NAME "ptcollab"
	Name "ptcollab ${VERSION}"
	OutFile "ptcollab-install-win.exe"
!else if "${ARCH}" == "x86"
	InstallDir "$PROGRAMFILES32\ptcollab"
	!define NAME "ptcollab (x86)"
	Name "ptcollab ${VERSION} (${ARCH})"
	OutFile "ptcollab-install-win-${ARCH}.exe"
!else
	!error "Architecture must be either 'amd64' or 'x86'."
!endif

!include "MUI2.nsh"

Unicode True
RequestExecutionLevel admin

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "header.bmp"
!define MUI_HEADERIMAGE_BITMAP_STRETCH AspectFitHeight
!define MUI_ICON "icon.ico"
!define MUI_UNICON "icon.ico"

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!define MUI_FINISHPAGE_SHOWREADME ""
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Create Start menu shortcut"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION StartMenuShortcut
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_LANGUAGE "English"

Section "Install"
	SetShellVarContext all

	SetOutPath "$INSTDIR"
	File "icon.ico"
	WriteUninstaller "$INSTDIR\uninstall.exe"
	File /r "${IN_PATH}\*"

	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ptcollab" "DisplayName" "${NAME}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ptcollab" "Publisher" "ptcollab"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ptcollab" "DisplayIcon" "$\"$INSTDIR\icon.ico$\""
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ptcollab" "InstallLocation" "$\"$INSTDIR$\""
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ptcollab" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ptcollab" "URLInfoAbout" "https://yuxshao.github.io/ptcollab/"
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ptcollab" "NoModify" 1
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ptcollab" "NoRepair" 1
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ptcollab" "DisplayVersion" "${VERSION}"

	WriteUninstaller "$INSTDIR\Uninstall.exe"
	CreateShortcut "$DESKTOP\${NAME}.lnk" "$INSTDIR\ptcollab.exe" "" "$INSTDIR\icon.ico"
SectionEnd

Function StartMenuShortcut
	SetShellVarContext all

	CreateShortcut "$SMPrograms\${NAME}.lnk" "$INSTDIR\ptcollab.exe"
FunctionEnd

Section "Uninstall"
	SetShellVarContext all

	Delete "$INSTDIR\uninstall.exe"
	Delete "$DESKTOP\${NAME}.lnk"
	Delete "$SMPROGRAMS\${NAME}.lnk"
	RMDir /r "$INSTDIR"
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ptcollab"
SectionEnd
