<#
  Build the standalone setup wizard: getv/wizard/setup_wizard.exe.

  A separate, small build from build_windows.ps1 on purpose. The wizard has to run BEFORE the
  decomp is cloned, before a ROM exists, and before goldeneye.exe can be linked -- its entire
  job is to drive tools/setup-windows.sh, which does those things. So it cannot be a target of
  the build it bootstraps, and does not touch build_windows.ps1 or share output with it.

  This is a developer-facing script, not something an end user runs. A developer with the same
  toolchain build_windows.ps1 already needs (mingw, SDL2, GLEW, Dear ImGui -- all from
  tools/fetch_deps_windows.ps1) runs this once; the resulting setup_wizard.exe is the candidate
  artifact. It contains no ROM-derived or decomp-derived code, which is the package's technical
  boundary. That check does not resolve or supersede the licensing review recorded in
  docs/LICENSING.md.

  USAGE
      powershell -NoProfile -File getv\build_wizard.ps1
      -Mingw : toolchain root (default C:\mingw64, matching fetch_deps_windows.ps1 and the
               first-run setup pipeline)
      -RepoUrl / -RepoRef : source repository and branch/tag the packaged wizard will install
#>
[CmdletBinding()]
param(
  [string]$Mingw = 'C:\mingw64',
  [string]$RepoUrl = 'https://github.com/seb-patron/goldeneye-native.git',
  [string]$RepoRef = 'main'
)

$ErrorActionPreference = 'Continue'
$here  = Split-Path -Parent $MyInvocation.MyCommand.Path   # getv/
$root  = Split-Path -Parent $here                           # repo root
$wiz   = Join-Path $here 'wizard'
$build = Join-Path $wiz 'build'
$imgui = Join-Path $env:USERPROFILE '.n64tvos\imgui-win'

$gcc = Join-Path $Mingw 'bin\gcc.exe'
$gxx = Join-Path $Mingw 'bin\g++.exe'
$windres = Join-Path $Mingw 'bin\windres.exe'
if (-not (Test-Path $gcc)) { throw "no gcc at $gcc -- run tools\fetch_deps_windows.ps1 first" }
if (-not (Test-Path $windres)) { throw "no windres at $windres -- run tools\fetch_deps_windows.ps1 first" }
if (-not (Test-Path (Join-Path $imgui 'lib\libimgui.a'))) {
  throw "no Dear ImGui at $imgui -- run tools\fetch_deps_windows.ps1 first"
}
if (-not (Test-Path (Join-Path $Mingw 'include\GL\glew.h'))) {
  throw "no GLEW headers under $Mingw -- run tools\fetch_deps_windows.ps1 first"
}

New-Item -ItemType Directory -Force -Path $build | Out-Null

# A branch package must clone the branch containing its matching setup pipeline, not whatever main
# happens to contain when a tester double-clicks it. Generate a tiny ignored header rather than
# fighting three layers of PowerShell/GCC quote removal on -D strings. Restrict the values before
# placing them in C source so neither a quote nor an option can escape the define.
if ($RepoUrl -notmatch '^https://github\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+(?:\.git)?$') {
  throw "unsupported wizard repository URL: $RepoUrl"
}
if ($RepoRef -notmatch '^[\p{L}\p{M}\p{N}][\p{L}\p{M}\p{N}._/+@-]*$' -or
    $RepoRef -match '\.\.' -or $RepoRef -match '@\{' -or $RepoRef.EndsWith('/')) {
  throw "unsupported wizard repository ref: $RepoRef"
}
$packageConfig = Join-Path $build 'package_config.h'
$packageConfigLines = @(
  "#define GETV_WIZARD_REPO_URL `"$RepoUrl`"",
  "#define GETV_WIZARD_REPO_REF `"$RepoRef`""
)
[IO.File]::WriteAllLines($packageConfig, $packageConfigLines, [Text.UTF8Encoding]::new($false))

# Same reason as the ImGui block in tools/fetch_deps_windows.ps1: assert and __FILE__ strings
# otherwise carry the absolute path this was built from, and the resulting binary is published
# for other people to download. $root is wherever the developer happened to clone, so it is
# mapped to a fixed token rather than shipped.
$cflags = @(
  '-std=c++17', '-O1', '-w',
  "-ffile-prefix-map=$root=goldeneye-native",
  "-fmacro-prefix-map=$root=goldeneye-native",
  "-I$wiz",
  "-I$root\getv\port\src",
  "-I$imgui\include",
  # WinLibs does not treat <toolchain-root>/include as a default search directory. The local
  # development toolchain happened to, which hid this until the first clean hosted build:
  # fetch_deps_windows.ps1 installed GL/glew.h correctly, but setup_wizard.cpp still could not
  # include it without an explicit root. SDL already has its narrower include below.
  "-I$Mingw\include",
  "-I$Mingw\include\SDL2",
  '-include', $packageConfig
)

Write-Output "== compiling =="
$objs = @()
$sources = @(
  @{ src = Join-Path $wiz 'setup_wizard.cpp';                    cxx = $true  },
  @{ src = Join-Path $wiz 'sha1.c';                               cxx = $false },
  @{ src = Join-Path $root 'getv\port\src\ge_icon_apply.c';       cxx = $false }
)
foreach ($s in $sources) {
  $o = Join-Path $build ((Split-Path -Leaf $s.src) + '.o')
  $cc = if ($s.cxx) { $gxx } else { $gcc }
  $out = & $cc @cflags -c $s.src -o $o 2>&1
  if ($LASTEXITCODE -ne 0 -or -not (Test-Path $o)) {
    $out | ForEach-Object { Write-Output $_ }
    throw "compile failed: $($s.src)"
  }
  Write-Output "  $(Split-Path -Leaf $s.src)"
  $objs += $o
}

# The wizard still uses narrow-character Win32 and CRT APIs. Windows 10 version 1903 and newer
# can make those APIs consume UTF-8 when the process opts in through an application manifest,
# which keeps non-ASCII install and selected-file paths intact. Compile the manifest as a resource
# rather than leaving it beside the executable, so the one-file setup contract remains true.
$resourceSource = Join-Path $wiz 'setup_wizard.rc'
$resourceObject = Join-Path $build 'setup_wizard_resource.o'
$out = & $windres --include-dir $wiz --input $resourceSource --output $resourceObject 2>&1
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $resourceObject)) {
  $out | ForEach-Object { Write-Output $_ }
  throw "resource compile failed: $resourceSource"
}
Write-Output "  setup_wizard.rc"
$objs += $resourceObject

# GCC's Windows specs append a fallback `default-manifest.o` to every executable. Leaving that in
# place beside our manifest creates two resources with the same RT_MANIFEST/name/language tuple,
# and the loader's choice would be ambiguous. Derive link-only specs from this pinned toolchain and
# remove that one fallback entry so the executable has exactly one authoritative manifest.
$defaultManifestSpec = '%{!shared:%:if-exists(default-manifest.o%s)}'
$toolchainSpecsOutput = @(& $gxx -dumpspecs 2>&1)
if ($LASTEXITCODE -ne 0) {
  $toolchainSpecsOutput | ForEach-Object { Write-Output $_ }
  throw 'could not read GCC specs while preparing the wizard manifest'
}
$toolchainSpecs = $toolchainSpecsOutput -join "`n"
if (-not $toolchainSpecs.Contains($defaultManifestSpec)) {
  throw 'GCC specs no longer contain the expected default-manifest entry'
}
$linkSpecs = Join-Path $build 'setup_wizard_link.specs'
Set-Content -LiteralPath $linkSpecs -Encoding ASCII -Value $toolchainSpecs.Replace($defaultManifestSpec, '')

Write-Output "== linking =="
$bin = Join-Path $build 'setup_wizard.exe'
Remove-Item $bin -Force -ErrorAction SilentlyContinue
# -s strips the symbol table. Nothing here is debugged from a shipped binary, and a symbol
# table is another place build paths survive.
# -static, not just -static-libgcc/-static-libstdc++. The README's Windows instructions tell a user
# to download the setup executable and double-click it -- one file, nothing else. The first build of
# this did not honour that: it imported SDL2.dll, libstdc++-6.dll and libwinpthread-1.dll, and a
# Windows program that cannot find a DLL does not say so. It exits with 0xC0000135, prints
# nothing, and leaves someone staring at a file that appears to do nothing when double-clicked.
#
# Measured while fixing it: run from a directory holding only the .exe, the dynamic build printed
# no output at all and still exited 0 through cmd's ERRORLEVEL, so a test that checked only the
# exit code called it a pass. Check for real output, not rc.
#
# The extra -l flags after SDL2 are what libSDL2.a itself needs once it is no longer a DLL
# (setupapi/version/uuid/cfgmgr32/hid for device enumeration, ole32/oleaut32/shell32 for COM and
# drag-drop, winmm for timers). The DLL copy that used to sit below this is gone with them.
$linkArgs = @("-specs=$linkSpecs", '-o', $bin, '-s', '-static') + $objs + @(
  (Join-Path $imgui 'lib\libimgui.a'),
  '-lglew32', '-lmingw32', '-lSDL2',
  '-lopengl32', '-lgdi32', '-limm32', '-ldbghelp', '-lcomdlg32', '-lole32',
  '-loleaut32', '-lshell32', '-lsetupapi', '-lversion', '-luuid', '-ladvapi32',
  '-lcfgmgr32', '-lhid', '-lwinmm', '-lm'
)
$out = & $gxx @linkArgs 2>&1
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $bin)) {
  $out | Select-Object -First 40 | ForEach-Object { Write-Output $_ }
  throw "LINK FAILED (gcc exit $LASTEXITCODE)"
}

# The point of -static is that this file stands alone, and the way that silently regresses is a
# library quietly going back to its import-library form. So ask the binary rather than trusting
# the flags: anything imported that is not a Windows system DLL means the download is broken for
# everyone who does not already have a mingw toolchain on their PATH.
$objdump = Join-Path $Mingw 'bin\objdump.exe'
if (Test-Path $objdump) {
  $sys = @('kernel32','user32','gdi32','advapi32','shell32','ole32','oleaut32','opengl32',
           'comdlg32','imm32','setupapi','version','winmm','uuid','cfgmgr32','hid','dbghelp',
           'msvcrt','ucrtbase','ws2_32','shlwapi','rpcrt4','crypt32','bcrypt','iphlpapi')
  $bad = @()
  foreach ($line in (& $objdump -p $bin | Select-String 'DLL Name:')) {
    $name = ($line -replace '.*DLL Name:\s*','').Trim()
    $stem = [IO.Path]::GetFileNameWithoutExtension($name).ToLower()
    if ($stem -like 'api-ms-*') { continue }
    if ($sys -notcontains $stem) { $bad += $name }
  }
  if ($bad.Count -gt 0) {
    Write-Output ("NOT STANDALONE -- still imports: " + ($bad -join ', '))
    throw "setup_wizard.exe is not self-contained; the Windows setup flow promises a single file"
  }
  Write-Output "imports: Windows system DLLs only, so the .exe ships on its own"
}

Write-Output ("wizard binary: {0} ({1:N1} MB)" -f $bin, ((Get-Item $bin).Length / 1MB))
