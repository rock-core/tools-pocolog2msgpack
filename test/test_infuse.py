from pocolog2msgpack import rock2infuse
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
    assert_equal(samples[0], [1, 2, 3, 0])


def test_convert_rigid_body_state():
    data = {
        "/component.port":
        [
            {
                "time": {
                    "microseconds": 5,
                },
                "sourceFrame": "A",
                "targetFrame": "B",
                "position": {"data": [0, 1, 2]},
                "cov_position": {"data": [0, 1, 2, 3, 4, 5, 6, 7, 8]},
                "orientation": {"re": 0, "im": [1, 2, 3]},
                "cov_orientation": {"data": [0, 1, 2, 3, 4, 5, 6, 7, 8]},
                "velocity": {"data": [2, 3, 4]},
                "cov_velocity": {"data": [0, 1, 2, 3, 4, 5, 6, 7, 8]},
                "angular_velocity": {"data": [3, 4, 5]},
                "cov_angular_velocity": {"data": [0, 1, 2, 3, 4, 5, 6, 7, 8]}
            }
        ],
        "/component.port.meta":
        {
            "type": "/base/samples/RigidBodyState",
            "timestamps": [5],
        }
    }
    data = rock2infuse(data)
    samples = data["/component.port"]
    assert_equal(samples[0]["timestamp"]["microseconds"], 5)
    assert_equal(samples[0]["sourceFrame"], "A")
    assert_equal(samples[0]["targetFrame"], "B")
    assert_equal(samples[0]["pos"], [0, 1, 2])
    assert_equal(samples[0]["cov_position"], [[0, 1, 2], [3, 4, 5], [6, 7, 8]])
    assert_equal(samples[0]["orient"], [1, 2, 3, 0])
    assert_equal(samples[0]["cov_orientation"], [[0, 1, 2], [3, 4, 5], [6, 7, 8]])
    assert_equal(samples[0]["velocity"], [2, 3, 4])
    assert_equal(samples[0]["cov_velocity"], [[0, 1, 2], [3, 4, 5], [6, 7, 8]])
    assert_equal(samples[0]["angular_velocity"], [3, 4, 5])
    assert_equal(samples[0]["cov_angular_velocity"], [[0, 1, 2], [3, 4, 5], [6, 7, 8]])


def test_convert_laser_scan():
    data = {
        "/component.port":
        [
            {
                "time": {"microseconds": 3},
                "start_angle": 0.0,
                "angular_resolution": 0.1,
                "speed": 0.1,
                "ranges": [0, 1, 2, 3],
                "minRange": 0,
                "maxRange": 3,
                "remission": [0, 1, 2, 3]
            }
        ],
        "/component.port.meta":
        {
            "type": "/base/samples/LaserScan",
            "timestamps": [5]
        }
    }
    data = rock2infuse(data)
    samples = data["/component.port"]
    assert_equal(samples[0]["ref_time"]["microseconds"], 3)
    assert_equal(samples[0]["start_angle"], 0.0)
    assert_equal(samples[0]["angular_resolution"], 0.1)
    assert_equal(samples[0]["speed"], 0.1)
    assert_equal(samples[0]["ranges"], [0, 1, 2, 3])
    assert_equal(samples[0]["minRange"], 0)
    assert_equal(samples[0]["maxRange"], 3)
    assert_equal(samples[0]["remission"], [0, 1, 2, 3])


def test_convert_pointcloud():
    data = {
        "/component.port":
        [
            {
                "time": {"microseconds": 3},
                "points": [{"data": [0, 1, 2]}, {"data": [2, 3, 4]}],
                "colors": [{"data": [255, 255, 255, 255]},
                           {"data": [255, 255, 255, 255]}]
            }
        ],
        "/component.port.meta":
        {
            "type": "/base/samples/Pointcloud",
            "timestamps": [5]
        }
    }
    data = rock2infuse(data)
    samples = data["/component.port"]
    assert_equal(
        samples[0]["metadata"]["timeStamp"]["microseconds"], 3)
    assert_equal(samples[0]["data"]["points"],
                 [[0, 1, 2], [2, 3, 4]])
    assert_equal(samples[0]["data"]["colors"],
                 [[255, 255, 255], [255, 255, 255]])
