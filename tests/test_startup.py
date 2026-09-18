import asyncio
import logging
import os
import tempfile
import unittest
from unittest import mock

from app.controller import listen as listener_module
from app.signaling.signal_client import SignalClient
import main


class BlockingConnect:
    def __await__(self):
        async def wait_forever():
            await asyncio.Event().wait()

        return wait_forever().__await__()


class FailingAsyncIterator:
    def __aiter__(self):
        return self

    async def __anext__(self):
        raise RuntimeError("listener failed")


class SignalClientTests(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        logging.basicConfig(handlers=[logging.NullHandler()], force=True)

    async def test_connect_times_out_and_cleans_up(self):
        client = SignalClient("room", "client", "https://example.test")

        with mock.patch(
            "app.signaling.signal_client.websockets.connect",
            return_value=BlockingConnect(),
        ):
            with self.assertRaises(asyncio.TimeoutError):
                await client.connect(timeout=0.01)

        self.assertIsNone(client.websocket)

    async def test_listener_reraises_unexpected_errors(self):
        client = SignalClient("room", "client", "https://example.test")
        client.websocket = FailingAsyncIterator()

        with self.assertRaisesRegex(RuntimeError, "listener failed"):
            await client.listen()


class ListenerRetryTests(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        logging.basicConfig(handlers=[logging.NullHandler()], force=True)

    async def test_connection_failure_is_retried_after_initial_delay(self):
        signal = mock.AsyncMock()
        signal.connect.side_effect = OSError("network unavailable")
        signal.close = mock.AsyncMock()

        with mock.patch.object(listener_module, "SignalClient", return_value=signal), \
               mock.patch.object(listener_module, "ReceiverHandler"), \
             mock.patch.object(
                 listener_module.asyncio,
                 "sleep",
                 side_effect=asyncio.CancelledError,
             ) as sleep:
            with self.assertRaises(asyncio.CancelledError):
                await listener_module.listen_bash_mode()

        signal.connect.assert_awaited_once_with(timeout=listener_module.CONNECT_TIMEOUT)
        signal.close.assert_awaited_once()
        sleep.assert_awaited_once_with(listener_module.INITIAL_RETRY_DELAY)


class LoggingTests(unittest.TestCase):
    def test_configure_logging_creates_user_log(self):
        with tempfile.TemporaryDirectory() as local_app_data:
            with mock.patch.dict(os.environ, {"LOCALAPPDATA": local_app_data}):
                log_path = main.configure_logging()
                logging.getLogger("argus-test").warning("startup diagnostic")
                logging.shutdown()

            self.assertEqual(
                log_path,
                os.path.join(local_app_data, "Argus", "argus.log"),
            )
            with open(log_path, encoding="utf-8") as log_file:
                self.assertIn("startup diagnostic", log_file.read())


if __name__ == "__main__":
    unittest.main()
