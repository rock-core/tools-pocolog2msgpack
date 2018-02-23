# TODO document


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
    if "data" in sample and len(sample) == 1:  # vectors
        return sample["data"]
    # TODO don't know how to handle matrices correctly...
    if "re" in sample and "im" in sample:  # quaternion w, x, y, z
        return [sample["re"]] + sample["im"]

    if "usecPerSec" in sample:
        del sample["usecPerSec"]

    for k in sample.keys():
        if isinstance(sample[k], dict):
            sample[k] = _translate_sample(sample[k])
    return sample
