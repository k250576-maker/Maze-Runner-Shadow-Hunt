$ErrorActionPreference = 'Stop'
$game = Join-Path $PSScriptRoot 'build\game.exe'
if (-not (Test-Path -LiteralPath $game)) {
    throw 'Game not found. Run the VS Code Build task first.'
}
$previousPath = $env:PATH
$previousGallium = $env:GALLIUM_DRIVER
$previousOverride = $env:MESA_LOADER_DRIVER_OVERRIDE
try {
    $env:PATH = "C:\msys64\ucrt64\bin;$previousPath"
    $env:GALLIUM_DRIVER = 'llvmpipe'
    $env:MESA_LOADER_DRIVER_OVERRIDE = 'llvmpipe'
    Push-Location $PSScriptRoot
    try {
        & $game
        $gameExitCode = $LASTEXITCODE
    } finally {
        Pop-Location
    }
} finally {
    $env:PATH = $previousPath
    $env:GALLIUM_DRIVER = $previousGallium
    $env:MESA_LOADER_DRIVER_OVERRIDE = $previousOverride
}
exit $gameExitCode
