# Build + run the offline SoundTouch output-stage test (test_audio_out.cpp).
# Usage: .\ps3recomp\libs\audio\tests\run_test_audio_out.ps1   (exit 0 = ALL PASSED)
# Build output goes to <port>\scratch\audiotest.
$ErrorActionPreference = 'Stop'
$t = $PSScriptRoot
$a = Split-Path $t
$st = Join-Path (Split-Path (Split-Path $a)) 'third_party\soundtouch'
$out = Join-Path (Split-Path (Split-Path (Split-Path $a))) 'scratch\audiotest'
New-Item -ItemType Directory -Force $out | Out-Null
$srcs = @("`"$t\test_audio_out.cpp`"", "`"$a\audio_out.cpp`"") + (Get-ChildItem "$st\source\SoundTouch\*.cpp" | ForEach-Object { "`"$($_.FullName)`"" })
$vc = 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul & set "PATH=C:\Program Files\LLVM\bin;%PATH%" & cd /d "' + $out + '" & '
$cmd = 'clang-cl /nologo /std:c++20 /EHsc /O2 -MD -w -DST_NO_EXCEPTION_HANDLING -DUSE_MULTICH_ALWAYS -DSOUNDTOUCH_FLOAT_SAMPLES -DSOUNDTOUCH_ALLOW_SSE ' +
       "-I`"$st\include`" $($srcs -join ' ') /Fetest_audio_out.exe /link /INCREMENTAL:NO 2>&1"
Remove-Item "$out\test_audio_out.exe" -ErrorAction SilentlyContinue
& $env:ComSpec /c ($vc + $cmd) | Select-String ' error' | Select-Object -First 5 | ForEach-Object Line
if (-not (Test-Path "$out\test_audio_out.exe")) { 'BUILD FAIL test_audio_out'; exit 1 }
& "$out\test_audio_out.exe"
exit $LASTEXITCODE
