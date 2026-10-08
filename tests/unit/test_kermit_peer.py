"""The protocol test peer must tolerate an ACK crossing a retransmission."""

import importlib.util
import os
import unittest
from pathlib import Path
from unittest.mock import Mock


@unittest.skipUnless(os.name == "posix", "Kermit PTY peer requires POSIX")
class KermitPeerTests(unittest.TestCase):
    def setUp(self):
        source = Path(__file__).resolve().parents[1] / "e2e/harness/test-kermit-server.py"
        spec = importlib.util.spec_from_file_location("kermit_peer", source)
        self.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)
        self.peer = self.module.Peer(-1)
        self.previous = self.module.packet(b"D", b"payload", 63, 3)
        self.peer.acknowledged_data = self.previous
        self.peer.send = Mock()

    def test_ack_crosses_retry_before_next_data_or_eof(self):
        for kind in (b"D", b"Z"):
            with self.subTest(kind=kind):
                self.peer.receive = Mock(
                    side_effect=[self.previous, self.module.packet(kind, b"", 0, 3)]
                )
                self.assertEqual(self.peer.expect(kind, 3, 0), b"")
                self.peer.send.assert_called_with(self.module.packet(b"Y", sequence=63, check=3))

    def test_changed_retransmission_is_rejected(self):
        self.peer.receive = Mock(return_value=self.module.packet(b"D", b"changed", 63, 3))
        with self.assertRaisesRegex(AssertionError, "unexpected packet"):
            self.peer.expect(b"Z", 3, 0)
        self.peer.send.assert_not_called()

    def test_duplicate_loop_is_bounded(self):
        self.peer.receive = Mock(return_value=self.previous)
        with self.assertRaisesRegex(AssertionError, "unexpected packet"):
            self.peer.expect(b"Z", 3, 0)
        self.assertEqual(self.peer.send.call_count, 8)
