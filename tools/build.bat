@echo off
cd /d D:\ESP32_PROJECT\sound_dsp_project
set IDF_PATH=D:\Espressif_vscode\frameworks\esp-idf-v5.5.2
set IDF_TOOLS_PATH=D:\Espressif_vscode\tools
set PATH=D:\Espressif_vscode\python_env\idf5.5_py3.11_env\Scripts;D:\Espressif_vscode\tools\ccache\4.11.2\ccache-4.11.2-windows-x86_64;%PATH%
D:\Espressif_vscode\python_env\idf5.5_py3.11_env\Scripts\python.exe D:\Espressif_vscode\frameworks\esp-idf-v5.5.2\tools\idf.py build > D:\ESP32_PROJECT\sound_dsp_project\build_out.txt 2> D:\ESP32_PROJECT\sound_dsp_project\build_err.txt
exit /b %ERRORLEVEL%
