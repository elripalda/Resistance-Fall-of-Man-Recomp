; Installer for the Resistance: Fall of Man native PC port.
; Build it with:  powershell -ExecutionPolicy Bypass -File tools\installer\build_installer.ps1
; The installer holds only the port's own files; the player's game copy is
; located on first run and prepared by Setup.bat.
; Third-party redistributables staged with the sources (port_common.iss copies
; the whole stage; build_installer.ps1 warns if one is missing):
;   tools\ffx_bin\  AMD FidelityFX DLLs (MIT)
;   tools\xess\     Intel XeSS libxess.dll + headers, Intel Simplified Software License (LICENSE.txt, third-party-programs.txt)
;   tools\nvapi\    NVIDIA NVAPI headers + nvapi64.lib for NVIDIA Reflex (MIT, License.txt)

#define PortName      "Resistance Fall of Man PC Port"
#define PortShort     "ResistancePort"
#define PortAppId     "{{6C1E6A0B-2F4D-4C55-9A4E-1F0A9B3D7E01}"
#define PortUrl       "https://github.com/miroslavdanutalexandru-alt/Resistance-Fall-of-Man-Recomp"
#define GameTitle     "Resistance: Fall of Man (US disc, BCUS98107)"
; UserDir: the folder under %USERPROFILE%\Saved Games for the settings backup
#define UserDir       "Resistance Fall of Man"

#include "port_common.iss"
