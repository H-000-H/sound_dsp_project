$idfPython = "D:\Espressif_vscode\python_env\idf5.5_py3.11_env\Scripts\python.exe"
$env:IDF_TOOLS_PATH = "D:\Espressif_vscode"
$env:IDF_PATH = "D:\Espressif_vscode\frameworks\esp-idf-v5.5.2"
$env:PATH = "D:\Espressif_vscode\python_env\idf5.5_py3.11_env\Scripts;D:\Espressif_vscode\tools\ccache\4.11.2\ccache-4.11.2-windows-x86_64;D:\Espressif_vscode\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\bin;$env:PATH"

# Remove MSYSTEM env var to bypass idf.py's unsupported environment check
Remove-Item -Path "env:MSYSTEM" -ErrorAction SilentlyContinue

Set-Location D:\ESP32_PROJECT\sound_dsp_project

Write-Output "=== Starting build ==="
$output = & $idfPython "$env:IDF_PATH\tools\idf.py" build 2>&1
$exitCode = $LASTEXITCODE
foreach ($line in $output) {
    $line
}
Write-Output "=== Exit code: $exitCode ==="
exit $exitCode
