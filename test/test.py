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


def test_help():
    proc = pexpect.spawn("pocolog2msgpack -h")
    proc.expect("Options:")
    proc.expect(pexpect.EOF)


def test_verbose():
    proc = pexpect.spawn("pocolog2msgpack -v 5")
    proc.expect("Verbosity level is 5")
    proc.expect(pexpect.EOF)


def test_convert_integers():
    output = "integers.msg"
    with cleanup(output):
        cmd = "pocolog2msgpack -l test/data/integers.0.log -o %s" % output
        proc = pexpect.spawn(cmd)
        proc.expect(pexpect.EOF)
        log = msgpack.unpack(open(output, "r"))
        assert_in("/messages.messages", log)
        messages = log["/messages.messages"]
        assert_equal(len(messages), 5)
        assert_equal(messages[0], 1337)


def test_convert_strings():
    output = "strings.msg"
    with cleanup(output):
        cmd = "pocolog2msgpack -l test/data/strings.0.log -o %s" % output
        proc = pexpect.spawn(cmd)
        proc.expect(pexpect.EOF)
        log = msgpack.unpack(open(output, "r"))
        assert_in("/messages.messages", log)
        messages = log["/messages.messages"]
        assert_equal(len(messages), 6)
        assert_equal(messages[0], "1337")


def test_convert_vector_of_int():
    output = "vector_int.msg"
    with cleanup(output):
        cmd = "pocolog2msgpack -l test/data/vector_int.0.log -o %s" % output
        proc = pexpect.spawn(cmd)
        proc.expect(pexpect.EOF)
        log = msgpack.unpack(open(output, "r"))
        assert_in("/message_producer.messages", log)
        messages = log["/message_producer.messages"]
        assert_equal(len(messages), 5)
        assert_equal(messages[0][0], 132)
        assert_equal(messages[0][1], 2054829)
        assert_equal(messages[0][2], -233235)


def test_convert_vector_of_str():
    output = "vector_str.msg"
    with cleanup(output):
        cmd = "pocolog2msgpack -l test/data/vector_string.0.log -o %s" % output
        proc = pexpect.spawn(cmd)
        proc.expect(pexpect.EOF)
        log = msgpack.unpack(open(output, "r"))
        assert_in("/message_producer.messages", log)
        messages = log["/message_producer.messages"]
        assert_equal(len(messages), 5)
        assert_equal(messages[0][0], "hello")
        assert_equal(messages[0][1], "world")
