from infuse import rock2infuse
from nose.tools import assert_equal, assert_in


def test_convert_nothing():
    data = rock2infuse({})
    assert_equal(data, {})


def test_convert_time():
    data = {
        "/component.port":
        [
            {
                "microseconds": 5,
                "usecPerSec": 1000000,
            }
        ],
        "/component.port.meta":
        {
            "type": "/base/Time",
            "timestamps": [5]
        }
    }
    data = rock2infuse(data)
    assert_in("/component.port", data)
    samples = data["/component.port"]
    assert_equal(len(samples), 1)
    assert_equal(len(samples[0]), 1)
    assert_equal(samples[0]["microseconds"], 5)
    assert_in("microseconds", samples[0])
    assert_in("/component.port.meta", data)
    meta = data["/component.port.meta"]
    assert_in("type", meta)
    assert_equal(meta["type"], "Time")
    assert_in("timestamps", meta)
    assert_equal(len(meta["timestamps"]), 1)


def test_convert_vector2d():
    data = {
        "/component.port":
        [
            {
                "data": [0, 1]
            }
        ],
        "/component.port.meta":
        {
            "type": "/base/Vector2d",
            "timestamps": [5]
        }
    }
    data = rock2infuse(data)
    samples = data["/component.port"]
    assert_equal(samples[0], [0, 1])


def test_convert_quaternion():
    data = {
        "/component.port":
        [
            {
                "re": 0,
                "im": [1, 2, 3]
            }
        ],
        "/component.port.meta":
        {
            "type": "/base/Quaterniond",
            "timestamps": [5]
        }
    }
    data = rock2infuse(data)
    samples = data["/component.port"]
    assert_equal(samples[0], [0, 1, 2, 3])
