import pexpect
import msgpack
from nose.tools import assert_in, assert_equal


def test_no_logfile():
    proc = pexpect.spawn("pocolog2msgpack")
    proc.expect("No logfile given")
    proc.expect(pexpect.EOF)


def test_convert_integers():
    proc = pexpect.spawn("pocolog2msgpack test/data/int.0.log")
    proc.expect(pexpect.EOF)
    log = msgpack.unpack(open("output.msg", "r"))
    assert_in("/messages.messages", log)
    messages = log["/messages.messages"]
    assert_equal(len(messages), 5)
    assert_equal(messages[0], 1337)
