from infuse import rock2infuse
from nose.tools import assert_equal


def test_convert_nothing():
    data = rock2infuse({})
    assert_equal(data, {})
