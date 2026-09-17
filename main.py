import asyncio
import sys
import winreg
from app.controller.listen import listen_bash_mode


APP_NAME = "Argus"

def enable_startup():
    exe_path = sys.executable  # Path to Argus.exe

    with winreg.OpenKey(
        winreg.HKEY_CURRENT_USER,
        r"Software\Microsoft\Windows\CurrentVersion\Run",
        0,
        winreg.KEY_READ | winreg.KEY_SET_VALUE,
    ) as key:
        try:
            current, _ = winreg.QueryValueEx(key, APP_NAME)
            if current == exe_path:
                return  # Already registered
        except FileNotFoundError:
            pass

        winreg.SetValueEx(key, APP_NAME, 0, winreg.REG_SZ, exe_path)

def main():
    enable_startup()
    asyncio.run(listen_bash_mode())

if __name__ == "__main__":
    main()