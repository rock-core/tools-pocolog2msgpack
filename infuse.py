# TODO document
import msgpack
from math import sqrt


def rock2infuse_logfile(input_filename, output_filename):
    with open(input_filename, "rb") as f:
        log = msgpack.unpack(f, encoding="utf8")
    log = rock2infuse(log)
    with open(output_filename, "wb") as f:
        msgpack.pack(log, f, encoding="utf8")


def rock2infuse(data):
    """WARNING: modifies input data!"""
    data = _translate_typenames(data)
    data = _translate_types(data)
    return data


def _translate_typenames(data):
    for k in data.keys():
        if not k.endswith(".meta"):
            continue

        if "type" in data[k]:
            data[k]["type"] = _map_typename(data[k]["type"])
    return data


def _map_typename(typename):
    if typename.endswith("_m"):
        typename = typename[:-2]
    return typename.split("/")[-1]


def _translate_types(data):
    for k in data.keys():
        if k.endswith(".meta"):
            continue

        samples = data[k]
        for t in range(len(samples)):
            samples[t] = _translate_sample(samples[t])
    return data


def _translate_sample(sample):
    if isinstance(sample, dict):
        sample = _translate_dict(sample)
    elif isinstance(sample, list):
        sample = _translate_list(sample)
    return sample


def _translate_dict(sample):
    converted, new_sample = _convert_time(sample)
    if converted:
        return new_sample
    converted, new_sample = _convert_vector(sample)
    if converted:
        return new_sample
    # TODO don't know how to handle matrices correctly...
    converted, new_sample = _convert_quaternion(sample)
    if converted:
        return new_sample
    converted, new_sample = _convert_rigid_body_state(sample)
    if converted:
        return new_sample
    sample = _translate_time_to_ref_time(sample)

    for k in sample.keys():
        if isinstance(sample[k], dict):
            sample[k] = _translate_sample(sample[k])
        elif isinstance(sample[k], list):
            sample[k] = [_translate_sample(el) for el in sample[k]]
    return sample


def _convert_time(sample):
    if "usecPerSec" in sample:
        return True, {"microseconds": sample["microseconds"]}
    else:
        return False, None


def _convert_vector(sample):
    if "data" in sample and len(sample) == 1:
        return True, sample["data"]
    else:
        return False, None


def _convert_quaternion(sample):
    if "re" in sample and "im" in sample:
        return True, [sample["re"]] + sample["im"]  # quaternion w, x, y, z
    else:
        return False, None


def _convert_rigid_body_state(sample):
    if "time" in sample and "sourceFrame" in sample and "position" in sample:
        new_sample = {
            "timestamp": _translate_sample(sample["time"]),
            "sourceFrame": sample["sourceFrame"],
            "targetFrame": sample["targetFrame"],
            "pos": _translate_sample(sample["position"]),
            "cov_position": _convert_square_matrix(sample["cov_position"])[1],
            "orient": _convert_quaternion(sample["orientation"])[1],
            "cov_orientation": _convert_square_matrix(sample["cov_orientation"])[1],
            "velocity": _translate_sample(sample["velocity"]),
            "cov_velocity": _convert_square_matrix(sample["cov_velocity"])[1],
            "angular_velocity": _translate_sample(sample["angular_velocity"]),
            "cov_angular_velocity": _convert_square_matrix(sample["cov_angular_velocity"])[1],
        }
        return True, new_sample
    else:
        return False, None


def _convert_square_matrix(sample):
    content = sample["data"]
    n_rows = int(sqrt(len(content)))
    assert n_rows * n_rows == len(content)
    matrix = [[content[i * n_rows + j]
               for j in range(n_rows)] for i in range(n_rows)]
    return True, matrix


def _translate_time_to_ref_time(sample):
    if "time" in sample:
        sample["ref_time"] = sample["time"]
        del sample["time"]
    return sample


def _translate_list(sample):
    return [_translate_sample(el) for el in sample]
