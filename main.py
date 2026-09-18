import asyncio
import sys
import os
from app.controller.listen import listen_bash_mode


APP_NAME = "Argus"

def get_app_dir():
    if getattr(sys, "frozen", False):
        return os.path.dirname(sys.executable)

def enable_startup():
    # Do nothing on Linux or macOS.
    if sys.platform != "win32":
        return

    import winreg
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
                print("ALREADY REGISTERED")
                return  # Already registered
        except FileNotFoundError:
            pass

        print("REGISTERED")
        winreg.SetValueEx(key, APP_NAME, 0, winreg.REG_SZ, exe_path)

async def restart_listener():
    while True:
        print("STARTING LISTENER...")

        try:
            await asyncio.wait_for(listen_bash_mode(), timeout=300)
        except asyncio.TimeoutError:
            print("5 MINUTES REACHED. RESTARTING LISTENER...")
        except Exception as e:
            print(f"LISTENER CRASHED: {e}")

        # Small delay before restarting (optional)
        await asyncio.sleep(1)
        
async def main_async():
    app_dir = get_app_dir()

    print("=" * 40)
    print("ARGUS STARTING")
    print("EXE: ", sys.executable)
    print("APP DIR: ", app_dir)
    print("CWD: ", os.getcwd())
    print("PATH: ", os.environ.get("PATH"))
    print("APPDATA: ", os.environ.get("LOCALAPPDATA"))
    print("=" * 40)

    enable_startup()

    print("WAITING FOR WINDOWS NETWORK FOR 30")
    await asyncio.sleep(30)
    await restart_listener()

def main():
    asyncio.run(main_async())

if __name__ == "__main__":
    main()
