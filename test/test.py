import pexpect


def test_no_logfile():
    proc = pexpect.spawn("pocolog2msgpack")
    proc.expect("No logfile given")
    proc.expect(pexpect.EOF)
