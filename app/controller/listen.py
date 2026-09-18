import asyncio
import logging
from app.core.client import client_id, username
from app.core.config import ROOM, HOST
from app.signaling.signal_client import SignalClient
from app.handler.remote_command_handler import RemoteCommandHandler as ReceiverHandler


logger = logging.getLogger(__name__)

CONNECT_TIMEOUT = 15
INITIAL_RETRY_DELAY = 5
MAX_RETRY_DELAY = 60


async def listen_bash_mode():
    retry_delay = INITIAL_RETRY_DELAY
    signal = SignalClient(ROOM, client_id, HOST, username=username)
    ReceiverHandler(signal)  # registers handler before messages arrive

    while True:
        try:
            await signal.connect(timeout=CONNECT_TIMEOUT)
            retry_delay = INITIAL_RETRY_DELAY
            logger.info("Listening as %s (%s)", username, client_id)
            await signal.listen()  # blocks while connected

        except asyncio.CancelledError:
            raise
        except Exception as exc:
            logger.exception("Signaling connection lost: %s", exc)
        finally:
            await signal.close()

        logger.info("Retrying signaling connection in %s seconds", retry_delay)
        await asyncio.sleep(retry_delay)
        retry_delay = min(retry_delay * 2, MAX_RETRY_DELAY)