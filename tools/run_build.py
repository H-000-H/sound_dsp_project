import subprocess, os, sys

os.chdir(r"D:\ESP32_PROJECT\sound_dsp_project")
env = os.environ.copy()
env["IDF_TOOLS_PATH"] = r"D:\Espressif_vscode"
env["IDF_PATH"] = r"D:\Espressif_vscode\frameworks\esp-idf-v5.5.2"
env["PATH"] = r"D:\Espressif_vscode\python_env\idf5.5_py3.11_env\Scripts;" + \
              r"D:\Espressif_vscode\tools\ccache\4.11.2\ccache-4.11.2-windows-x86_64;" + \
              r"D:\Espressif_vscode\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\bin;" + \
              env["PATH"]
env.pop("MSYSTEM", None)

result = subprocess.run(
    [r"D:\Espressif_vscode\python_env\idf5.5_py3.11_env\Scripts\python.exe",
     r"D:\Espressif_vscode\frameworks\esp-idf-v5.5.2\tools\idf.py", "build"],
    capture_output=True, text=True, env=env
)
# Print stdout
sys.stdout.write(result.stdout)
# Print stderr if any
if result.stderr:
    sys.stderr.write(result.stderr)
sys.exit(result.returncode)
