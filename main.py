import asyncio
import logging
import os
import sys
import tempfile
from app.controller.listen import listen_bash_mode


APP_NAME = "Argus"
logger = logging.getLogger(__name__)

def get_app_dir():
    if getattr(sys, "frozen", False):
        return os.path.dirname(sys.executable)
    return os.path.dirname(os.path.abspath(__file__))


def configure_logging():
    local_app_data = os.environ.get("LOCALAPPDATA")
    if local_app_data:
        log_dir = os.path.join(local_app_data, APP_NAME)
    else:
        log_dir = os.path.join(tempfile.gettempdir(), APP_NAME)

    try:
        os.makedirs(log_dir, exist_ok=True)
        log_path = os.path.join(log_dir, "argus.log")
        handler = logging.FileHandler(log_path, encoding="utf-8")
    except OSError:
        log_path = os.path.join(get_app_dir(), "argus.log")
        handler = logging.FileHandler(log_path, encoding="utf-8")

    formatter = logging.Formatter(
        "%(asctime)s %(levelname)s %(name)s: %(message)s"
    )
    handler.setFormatter(formatter)
    logging.basicConfig(level=logging.INFO, handlers=[handler], force=True)
    logger.info("Logging initialized at %s", log_path)
    return log_path

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
                logger.info("Startup registration already exists")
                return  # Already registered
        except FileNotFoundError:
            pass

        logger.info("Startup registration created")
        winreg.SetValueEx(key, APP_NAME, 0, winreg.REG_SZ, exe_path)

async def restart_listener():
    while True:
        logger.info("Starting listener watchdog")

        try:
            await asyncio.wait_for(listen_bash_mode(), timeout=300)
        except asyncio.TimeoutError:
            logger.warning("Listener watchdog timeout; restarting listener")
        except Exception as e:
            logger.exception("Listener watchdog crashed: %s", e)

        # Small delay before restarting (optional)
        await asyncio.sleep(1)
        
async def main_async():
    app_dir = get_app_dir()

    logger.info("Argus starting; executable=%s app_dir=%s cwd=%s", sys.executable, app_dir, os.getcwd())

    enable_startup()
    await restart_listener()

def main():
    configure_logging()
    asyncio.run(main_async())

if __name__ == "__main__":
    main()
