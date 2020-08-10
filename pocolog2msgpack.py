import msgpack
import mmap
from math import sqrt


def object2relational(input_filename, output_filename, whitelist=()):
    """Convert a MsgPack logfile from object-oriented to relational storage.

    The log data produced by pocolog2msgpack can usually be accessed with
    log[port_name][sample_idx][field_a]...[field_b]. This is an
    object-oriented view of the data because you can easily access a whole
    object from the log. This is not convenient if you want to use the data
    for, e.g., machine learning, where you typically need the whole dataset
    in a 2D array, i.e. a relational view on the data, in which you can
    access data in the form log[port_name][feature][sample_idx].

    Parameters
    ----------
    input_filename : str
        Name of the original logfile

    output_filename : str
        Name of the converted logfile

    whitelist : list or tuple
        Usually arrays and vectors (represented as lists in Python) are handled
        as basic types and are put in a single column because they can have
        dynamic sizes. This is a list of fields that will be scanned
        recursively and interpreted as arrays with a fixed length. Note that
        you only have to give the name of the field, not the port name.
        An example would be ["elements", "names"] if you want fully unravel
        a JointState object.
    """
    with open(input_filename, "rb") as f:
        log = msgpack.unpack(f, encoding="utf8")
    port_names = [k for k in log.keys() if not k.endswith(".meta")]
    converted_log = dict()
    for port_name in port_names:
        if len(log[port_name]) == 0:
            continue
        all_keys = _extract_keys(log[port_name][0], whitelist)
        _convert_data(converted_log, log, port_name, all_keys)
        _convert_metadata(converted_log, log, port_name)
    with open(output_filename, "wb") as f:
        msgpack.pack(converted_log, f)


def _extract_keys(sample, whitelist=(), keys=()):
    if isinstance(sample, dict):
        result = []
        for k in sample.keys():
            result.extend(_extract_keys(sample[k], whitelist, keys + (k,)))
        return result
    elif isinstance(sample, list) and (".".join(keys) in whitelist):
        result = []
        for i in range(len(sample)):
            result.extend(_extract_keys(sample[i], whitelist, keys + (i,)))
        return result
    else:
        return [keys]


def _convert_data(converted_log, log, port_name, all_keys):
    converted_log[port_name] = dict()
    for keys in all_keys:
        new_key = ".".join(map(str, keys))
        if new_key == "":
            new_key = "data"
        converted_log[port_name][new_key] = []
        for t in range(len(log[port_name])):
            value = log[port_name][t]
            for k in keys:
                value = value[k]
            converted_log[port_name][new_key].append(value)


def _convert_metadata(converted_log, log, port_name):
    metadata = log[port_name + ".meta"]
    converted_log[port_name]["timestamp"] = metadata["timestamps"]
    n_rows = len(metadata["timestamps"])
    converted_log[port_name]["type"] = [metadata["type"]] * n_rows


def rock2infuse_logfile(input_filename, output_filename, verbose=0):
    """Convert a MsgPack logfile from rock log format to infuse format.

    Parameters
    ----------
    input_filename : str
        Name of the original logfile

    output_filename : str
        Name of the converted logfile

    verbose : int, optional (default: 0)
        Verbosity level
    """
    with open(input_filename, "rb") as inf:
        with mmap.mmap(inf.fileno(), 0, access=mmap.ACCESS_READ) as m:
            with open(output_filename, "wb") as outf:
                u = msgpack.Unpacker(inf, encoding="utf8")
                p = msgpack.Packer(encoding="utf8")
                _convert(u, outf, p, verbose)


def _convert(u, outf, p, verbose):
    n_keys = u.read_map_header()
    if verbose:
        print("Logfile [%d streams]" % n_keys)
    outf.write(p.pack_map_header(n_keys))
    for _ in range(n_keys):
        key = u.unpack()
        if verbose:
            print("  Stream [%s]" % key)
        outf.write(p.pack(key))
        if key.endswith(".meta"):
            _convert_meta_stream(u, outf, p, verbose)
        else:
            _convert_stream(u, outf, p, verbose)


def _convert_meta_stream(u, outf, p, verbose):
    n_keys = u.read_map_header()
    outf.write(p.pack_map_header(n_keys))
    for _ in range(n_keys):
        key = u.unpack()
        if verbose > 1:
            print("    Meta [%s]" % key)
        outf.write(p.pack(key))
        if key == "timestamps":
            _convert_array(u, outf, p, verbose=0)
        elif key == "type":
            typename = u.unpack()
            infuse_typename = _map_typename(typename)
            if verbose > 1:
                print("    [%s -> %s]" % (typename, infuse_typename))
            outf.write(p.pack(infuse_typename))
        else:
            entry = u.unpack()
            outf.write(p.pack(entry))


def _convert_stream(u, outf, p, verbose):
    _convert_array(
        u, outf, p, entry_converter=_translate_sample, verbose=verbose)


def _convert_array(u, outf, p, entry_converter=None, verbose=0):
    n_entries = u.read_array_header()
    outf.write(p.pack_array_header(n_entries))
    for i in range(n_entries):
        if verbose > 1:
            print("    Converting [%d/%d]" % (i + 1, n_entries))
        entry = u.unpack()
        if entry_converter is not None:
            entry = entry_converter(entry)
        outf.write(p.pack(entry))
        del entry


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
    if typename.startswith("/base"):
        typename = typename.split("/")[-1]
    if typename == "Frame":
        typename = "Image"
    return typename


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
    # TODO fix timestamp / ref_time inconsistencies when we switch to new
    #      version of ESROCOS' types
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
    converted, new_sample = _convert_pointcloud(sample)
    if converted:
        return new_sample
    converted, new_sample = _convert_depth_map(sample)
    if converted:
        return new_sample
    converted, new_sample = _convert_frame(sample)
    if converted:
        return new_sample
    converted, new_sample = _convert_rigid_body_state(sample)
    if converted:
        return new_sample
    converted, new_sample = _convert_joints(sample)
    if converted:
        return new_sample
    converted, new_sample = _convert_imusensors(sample)
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
        return True, sample["im"] + [sample["re"]]  # quaternion x, y, z, w
    else:
        return False, None


def _convert_depth_map(sample):
    if "vertical_projection" in sample:
        new_sample = {
            "ref_time": sample["time"],
            "timestamps": sample["timestamps"],
            "vertical_projection": sample["vertical_projection"].lower(),
            "horizontal_projection": sample["horizontal_projection"].lower(),
            "vertical_interval": sample["vertical_interval"],
            "horizontal_interval": sample["horizontal_interval"],
            "vertical_size": sample["vertical_size"],
            "horizontal_size": sample["horizontal_size"],
            "distances": sample["distances"],
            "remissions": sample["remissions"],
        }
        return True, new_sample
    else:
        return False, None


def _convert_frame(sample):
    if "frame_mode" in sample:
        new_sample = {
            "frame_time": sample["time"],
            "received_time": sample["received_time"],
            "attributes": _convert_frame_attributes(sample["attributes"]),
            "image": sample["image"],
            "datasize": sample["size"],
            "data_depth": sample["data_depth"],
            "pixel_size": sample["pixel_size"],
            "row_size": sample["row_size"],
            "frame_mode": sample["frame_mode"].lower(),
            "frame_status": sample["frame_status"].lower(),
        }
        return True, new_sample
    else:
        return False, None


def _convert_frame_attributes(frame_attributes):
    result = []
    for fa in frame_attributes:
        result.append({
            "att_name": fa["name_"],
            "data": fa["data_"]
        })
    return result


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


def _convert_joints(sample):
    if "time" in sample and "names" in sample and "elements" in sample:
        new_sample = {
            "timestamp": _translate_sample(sample["time"]),
            "names": sample["names"],
            "elements": sample["elements"]
        }
        return True, new_sample
    else:
        return False, None


def _convert_imusensors(sample):
    if "gyro" in sample:
        new_sample = {
            "timestamp": _translate_sample(sample["time"]),
            "acc": _translate_sample(sample["acc"]),
            "gyro": _translate_sample(sample["gyro"]),
            "mag": _translate_sample(sample["mag"]),
        }
        return True, new_sample
    else:
        return False, None


def _convert_pointcloud(sample):
    if "points" in sample:
        new_sample = {
            "metadata":
            {"timeStamp": _translate_sample(sample["time"])},
            "data":
            {"points": _translate_sample(sample["points"]),
             "colors": [p[:3] for p in
                        _translate_sample(sample["colors"])]
            }
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
    if "time" in sample and "latitude" not in sample:  # Exception: /gps/Solution
        sample["ref_time"] = sample["time"]
        del sample["time"]
    return sample


def _translate_list(sample):
    return [_translate_sample(el) for el in sample]
