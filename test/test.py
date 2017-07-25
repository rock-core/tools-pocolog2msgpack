import pexpect
import msgpack
import os
from contextlib import contextmanager
from nose.tools import assert_in, assert_equal


@contextmanager
def cleanup(output):
    try:
        yield
    finally:
        if os.path.exists(output):
            os.remove(output)


def test_no_logfile():
    proc = pexpect.spawn("pocolog2msgpack")
    proc.expect("No logfile given")
    proc.expect(pexpect.EOF)


def test_convert_integers():
    with cleanup("integers.msg"):
        cmd = "pocolog2msgpack -l test/data/integers.0.log -o integers.msg"
        proc = pexpect.spawn(cmd)
        proc.expect(pexpect.EOF)
        log = msgpack.unpack(open("integers.msg", "r"))
        assert_in("/messages.messages", log)
        messages = log["/messages.messages"]
        assert_equal(len(messages), 5)
        assert_equal(messages[0], 1337)


def test_convert_strings():
    with cleanup("strings.msg"):
        cmd = "pocolog2msgpack -l test/data/strings.0.log -o strings.msg"
        proc = pexpect.spawn(cmd)
        proc.expect(pexpect.EOF)
        log = msgpack.unpack(open("strings.msg", "r"))
        assert_in("/messages.messages", log)
        messages = log["/messages.messages"]
        assert_equal(len(messages), 6)
        assert_equal(messages[0], "1337")


def test_convert_vector_of_int():
    with cleanup("vector_of_int.msg"):
        cmd = "pocolog2msgpack -l test/data/vector_int.0.log -o vector_of_int.msg"
        proc = pexpect.spawn(cmd)
        proc.expect(pexpect.EOF)
        log = msgpack.unpack(open("vector_of_int.msg", "r"))
        assert_in("/message_producer.messages", log)
        messages = log["/message_producer.messages"]
        assert_equal(len(messages), 5)
        assert_equal(messages[0][0], 132)
        assert_equal(messages[0][1], 2054829)
        assert_equal(messages[0][2], -233235)
