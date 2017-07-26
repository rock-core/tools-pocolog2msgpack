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


def test_convert_rigid_body_state():
    output = "rigid_body_state.msg"
    with cleanup(output):
        cmd = "pocolog2msgpack -l test/data/rigid_body_state.0.log -o %s" % output
        proc = pexpect.spawn(cmd)
        proc.expect(pexpect.EOF)
        log = msgpack.unpack(open(output, "r"))
        assert_in("/message_producer.messages", log)
        messages = log["/message_producer.messages"]
        assert_equal(len(messages), 2)
        assert_equal(messages[0]["time"]["microseconds"], 0)
        assert_equal(messages[0]["sourceFrame"], "source")
        assert_equal(messages[0]["targetFrame"], "target")
        assert_equal(messages[0]["position"]["data"][0], 1.23)
        assert_equal(messages[0]["position"]["data"][1], 2.52)
        assert_equal(messages[0]["position"]["data"][2], 2.13)
        assert_equal(messages[0]["orientation"]["re"], 2.0)
        assert_equal(messages[0]["orientation"]["im"][0], 1.0)
        assert_equal(messages[0]["orientation"]["im"][1], 5.0)
        assert_equal(messages[0]["orientation"]["im"][2], -2.0)
        cov_position = messages[0]["cov_position"]
        assert_equal(cov_position["data"][0], 1.0)
        assert_equal(cov_position["data"][1], 0.0)
        assert_equal(cov_position["data"][2], 0.0)
        assert_equal(cov_position["data"][3], 0.0)
        assert_equal(cov_position["data"][4], 1.0)
        assert_equal(cov_position["data"][5], 0.0)
        assert_equal(cov_position["data"][6], 0.0)
        assert_equal(cov_position["data"][7], 0.0)
        assert_equal(cov_position["data"][8], 1.0)
