# VQEAF OS - offline build. Execute from any working directory.
param(
    [string]$PioHome = "",
    [string]$Pio = "",
    [string]$ReportDir = "",
    [switch]$DryRun,
    [switch]$CheckOnly,
    [ValidateSet('none','board','platformio','toolchain','library','clean','compiler','linker','network','missing-bin','buildfs')]
    [string]$SimulateFailure = 'none',
    [switch]$BuildFS,
    [switch]$HostTests,
    [switch]$Incremental,
    [switch]$VerboseBuild
)
$ErrorActionPreference = 'Stop'
Push-Location (Resolve-Path (Join-Path $PSScriptRoot '..'))
try {
    $pythonCmd = if (Get-Command py -ErrorAction SilentlyContinue) { @('py', '-3') }
                 elseif (Get-Command python -ErrorAction SilentlyContinue) { @('python') }
                 else { Write-Error 'Python 3 not found'; exit 2 }
    $opts = @('tools/build_offline.py')
    if ($PioHome) { $opts += @('--pio-home', $PioHome) }
    if ($Pio) { $opts += @('--pio', $Pio) }
    if ($ReportDir) { $opts += @('--report-dir', $ReportDir) }
    if ($DryRun) { $opts += '--dry-run' }
    if ($CheckOnly) { $opts += '--check-only' }
    if ($SimulateFailure -ne 'none') { $opts += @('--simulate-failure', $SimulateFailure) }
    if ($BuildFS) { $opts += '--buildfs' }
    if ($HostTests) { $opts += '--host-tests' }
    if ($Incremental) { $opts += '--incremental' }
    if ($VerboseBuild) { $opts += '--verbose' }
    if ($pythonCmd.Count -eq 2) { & $pythonCmd[0] $pythonCmd[1] @opts }
    else { & $pythonCmd[0] @opts }
    exit $LASTEXITCODE
} finally { Pop-Location }
