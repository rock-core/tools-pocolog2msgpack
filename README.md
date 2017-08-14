# pocolog2msgpack

Logfile converter from Rock's pocolog format to MessagePack.

## Motivation

Why do we need a converter from
[pocolog](https://github.com/rock-core/tools-pocolog) to
[MsgPack](http://msgpack.org)?

* We need a log representation that is independent of the used RCOS to
  handle all RCOS in the same code.
* The CSV export of pocolog / typelib is useless, it does not export
  strings correctly (strings are included e.g. in RigidBodyState), or any
  other binary container format like vectors.
* Pure text representations like CSV, JSON and YAML do not handle binary
  data like floats very well: they require too much memory and are not
  represented exactly.
* MsgPack is a nice representation that stores binary data efficiently and
  correctly. It can handle hierarchical datatypes like compounds, vectors,
  and even strings. :)
* There are MsgPack libraries for almost any language.

## Install

Dependencies are

* pocolog_cpp
* typelib
* boost
* [msgpack-c](https://github.com/msgpack/msgpack-c), built from current sources

You can install the dependencies manually and install this tool with

    mkdir build
    cd build
    cmake -DCMAKE_INSTALL_PREFIX=<install prefix> ..
    make install

The more convenient way is to use autoproj. You must add three lines to
your manifest:

    package_sets:
       ...
       - type: git
         url: git@git.hb.dfki.de:InFuse/infuse-package_set.git

    layout:
       ...
       - tools/pocolog2msgpack
       ...

## Usage

This repository contains a program that will convert between both log
formats. It is called `pocolog2msgpack`. These are its options:

```
Options:
  -h [ --help ]                         Print help message
  -v [ --verbose ] arg (=0)             Verbosity level
  -l [ --logfile ] arg                  Logfile
  -o [ --output ] arg (=output.msg)     Output file
  -s [ --size ] arg (=8)                Length of the size type. This should be
                                        8 for most machines, but it can be 1, 
                                        e.g. on robots.
  -c [ --container-limit ] arg (=10000) Maximum lenght of a container that will
                                        be converted. This option should only 
                                        be used if you have old logfiles from 
                                        which we can't read the container size 
                                        properly.
```

## Loading Logs in Python

Loading the converted logfiles in Python is as simple as those two lines:

```python
import msgpack
log = msgpack.unpack(open("output.msg", "r"))
```

The object `log` is a Python dictionary that contains names of all logged ports
as keys and the logged data in a list as its keys.

The logdata itself is stored at the key `"/<task_name>.<port name>"` and
meta data is stored at `"/<task_name>.<port name>.meta"`. The meta data
contains the timestamp for each sample at the key `"timestamps"` and the
data type at the key `"type"`. At the moment, we use the type names from
Typelib.

There are several examples of how to use the tool and how to load data in
Python in the test folder.

## Conversion to Relational Format

There is an optional Python package 'pocolog2msgpack' that will be installed
only if you have Python on your system. It can be used to convert data
from object-oriented format to a relational format so that you can convert it
directly to a [pandas.DataFrame](http://pandas.pydata.org/):

```python
import msgpack
import pocolog2msgpack
import pandas
pocolog2msgpack.object2relational(logfile, logfile_relational)
log = msgpack.unpack(open(logfile_relational, "r"))
df = pandas.DataFrame(log[port_name])
df.set_index("timestamp", inplace=True)  # use the timestamp as an index
```

Vectors or arrays will usually not be unravelled automatically even if they
have the same size in each sample. You have to whitelist them manually, e.g.

```python
pocolog2msgpack.object2relational(
    logfile, logfile_relational, whitelist=["elements", "names"])
```

This will result in unravelled base::samples::Joints object in this case:

```
{
    '/message_producer.messages':
    {
        'elements.1.position': [2.0, 2.0],
        'time.microseconds': [1502180215405251, 1502180216405234],
        'elements.1.raw': [nan, nan],
        'elements.0.raw': [nan, nan],
        'timestamp': [1502180215405385, 1502180216405284],
        'elements.0.acceleration': [nan, nan],
        'elements.1.speed': [nan, nan],
        'names.1': ['j2', 'j2'],
        'elements.1.acceleration': [nan, nan],
        'names.0': ['j1', 'j1'],
        'elements.0.speed': [nan, nan],
        'elements.1.effort': [nan, nan],
        'elements.0.position': [1.0, 1.0],
        'type': ['/base/samples/Joints', '/base/samples/Joints'],
        'elements.0.effort': [nan, nan]
    }
}
```