AMD FidelityFX SDK 2.3.0 API headers

Source: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK, tag v2.3.0
(commit 60f4ea81909200d8542eca14dccb2628b763a9a3), directory Kits/FidelityFX.
The files are unmodified copies of the upstream ones, in the upstream relative layout.
LICENSE.md is Kits/FidelityFX/docs/license.md. It lists these headers under the MIT licence.

They match the amd_fidelityfx_*_dx12.dll 2.3.0 binaries that setup.ps1 downloads into tools\ffx_bin.
rsx_fsr.inc (FSR 3 upscaling) and rsx_fsr3fg.inc (FSR 3 frame generation) use them.
rsx_fsr3fg.inc static_asserts the x64 struct layouts it relies on.
