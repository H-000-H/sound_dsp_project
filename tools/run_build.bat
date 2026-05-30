@echo off
setlocal
set "IDF_TOOLS_PATH=D:\Espressif_vscode"
set "IDF_PATH=D:\Espressif_vscode\frameworks\esp-idf-v5.5.2"
set "PATH=D:\Espressif_vscode\python_env\idf5.5_py3.11_env\Scripts;D:\Espressif_vscode\tools\ccache\4.11.2\ccache-4.11.2-windows-x86_64;D:\Espressif_vscode\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\bin;%PATH%"
set "MSYSTEM="
cd /d D:\ESP32_PROJECT\sound_dsp_project
D:\Espressif_vscode\python_env\idf5.5_py3.11_env\Scripts\python.exe D:\Espressif_vscode\frameworks\esp-idf-v5.5.2\tools\idf.py build 2>&1
echo EXIT_CODE=%ERRORLEVEL%
