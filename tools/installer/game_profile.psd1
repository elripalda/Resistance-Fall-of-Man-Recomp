# What this port needs from the player's game copy. Read by tools\installer\gamefiles.ps1
# (used by setup.ps1 and the installer). Paths are relative to the folder that contains PS3_GAME.
@{
    GameName    = 'Resistance: Fall of Man'
    TitleId     = 'BCUS98107'
    RegionLabel = 'US'
    AppVer      = '01.00'
    TitleMatch  = 'Fall of Man'                  # other regions of the same game (for a clearer message)
    Required    = @('PS3_GAME/PARAM.SFO', 'PS3_GAME/USRDIR/EBOOT.BIN', 'PS3_GAME/USRDIR/game.self')
    Program     = @('PS3_GAME/USRDIR/EBOOT.BIN', 'PS3_GAME/USRDIR/game.self')   # PS3 program files Setup decrypts
    Sha1        = @{
        'PS3_GAME/USRDIR/EBOOT.BIN' = '6a98d90e82f01a4a4a27a350b4dfc2f78fecf971'
        'PS3_GAME/USRDIR/game.self' = '0550538f1d5f2f8a8272e5e4018ece25f72f91f4'
    }
    MinBytes    = 16000000000                    # the full PS3_GAME folder is 16.5 GB
    SizeLabel   = '16.5 GB'
    BuildBytes  = 8000000000                     # generated code, the build and the shader cache
    SaveFolder  = 'Resistance Fall of Man'       # %USERPROFILE%\Saved Games\<this>
}
