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

async def restart_listener():
    while True:
        print("Starting listener...")

        try:
            await asyncio.wait_for(listen_bash_mode(), timeout=600)
        except asyncio.TimeoutError:
            print("10 minutes reached. Restarting listener...")
        except Exception as e:
            print(f"Listener crashed: {e}")

        # Small delay before restarting (optional)
        await asyncio.sleep(1)

def main():
    enable_startup()
    asyncio.run(restart_listener())

if __name__ == "__main__":
    main()