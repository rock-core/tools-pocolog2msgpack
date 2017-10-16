import msgpack


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
