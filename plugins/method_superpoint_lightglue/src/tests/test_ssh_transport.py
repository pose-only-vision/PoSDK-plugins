"""Transport tests do not connect to a server or import model dependencies."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import types
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('sparse_transport', ROOT / 'superpoint_lightglue_ssh_runner.py')
transport = importlib.util.module_from_spec(spec)
spec.loader.exec_module(transport)


class SparseSSHTransportTest(unittest.TestCase):
    def args(self, **kwargs):
        values = dict(ssh_target='remoteMAC', ssh_port=0, ssh_connect_timeout=15,
                      ssh_run_timeout=3600, ssh_result_limit_mb=8, ssh_identity_file='')
        values.update(kwargs)
        return types.SimpleNamespace(**values)

    def stream(self, payload=b'model-result', **kwargs):
        row = dict(name='result.bin', size=len(payload), sha256=hashlib.sha256(payload).hexdigest())
        row.update(kwargs)
        header = dict(protocol=transport.PROTOCOL, job_id='a'*32, image_count=2, files=[row])
        return io.BytesIO(json.dumps(header).encode()+b'\n'+payload)

    def test_download_verifies_and_commits(self):
        with tempfile.TemporaryDirectory() as d:
            output=Path(d)/'result.bin'
            transport.receive_results(self.stream(),output,2,'a'*32,1024)
            self.assertEqual(output.read_bytes(),b'model-result')

    def test_corrupt_download_preserves_previous_result(self):
        with tempfile.TemporaryDirectory() as d:
            output=Path(d)/'result.bin';output.write_bytes(b'previous')
            with self.assertRaisesRegex(ValueError,'checksum'):
                transport.receive_results(self.stream(sha256='0'*64),output,2,'a'*32,1024)
            self.assertEqual(output.read_bytes(),b'previous')

    def test_truncated_download_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(ValueError,'Truncated'):
                transport.receive_results(self.stream(size=50),Path(d)/'result.bin',2,'a'*32,1024)

    def test_result_path_escape_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(ValueError,'Unexpected'):
                transport.receive_results(self.stream(name='../escape'),Path(d)/'result.bin',2,'a'*32,1024)

    def test_job_identity_is_bound(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(ValueError,'identity'):
                transport.receive_results(self.stream(),Path(d)/'result.bin',2,'b'*32,1024)

    def test_symlink_destination_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            output=Path(d)/'result.bin';target=Path(d)/'other';target.write_bytes(b'previous')
            output.symlink_to(target)
            with self.assertRaisesRegex(ValueError,'symlink'):
                transport.receive_results(self.stream(),output,2,'a'*32,1024)
            self.assertEqual(target.read_bytes(),b'previous')

    def test_ssh_rejects_option_and_shell_injection(self):
        for target in ('-oProxyCommand=evil','host;touch /tmp/evil','$(whoami)','host\ncommand'):
            with self.subTest(target=target),self.assertRaises(ValueError):
                transport.ssh_arguments(self.args(ssh_target=target))

    def test_ssh_requires_existing_host_key_and_no_forwarding(self):
        args=transport.ssh_arguments(self.args())
        for option in ('StrictHostKeyChecking=yes','BatchMode=yes','ForwardAgent=no','ClearAllForwardings=yes'):
            self.assertIn(option,args)

    def test_remote_python_is_shell_quoted(self):
        value=transport.remote_executable('~/my runtime/python;echo unsafe')
        self.assertEqual(value,'"$HOME"/\'my runtime/python;echo unsafe\'')


if __name__=='__main__':
    unittest.main()
