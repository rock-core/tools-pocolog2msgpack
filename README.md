[![CircleCI Status](https://circleci.com/gh/rock-core/tools-pocolog2msgpack/tree/master.svg?style=shield&circle-token=:circle-token)](https://circleci.com/gh/rock-core/tools-pocolog2msgpack)

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
       - github: rock-core/package_set

    layout:
       ...
       - tools/pocolog2msgpack
       ...

**Important** when installing the library through Autproj, the Python part of this library and the correspondent python dependencies will only get installed if in your Autoproj settings the USE_PYTHON flag is set to true.

## Usage

This repository contains a program that will convert files from the pocolog log format to MessagePack format. It is called `pocolog2msgpack`. These are its options:

```
Options:
  -h [ --help ]                         Print help message
  -v [ --verbose ] arg (=0)             Verbosity level
  -l [ --logfile ] arg                  Logfiles
  -o [ --output ] arg (=output.msg)     Output file
  -c [ --container-limit ] arg (=1000000)
                                        Maximum length of a container that will
                                        be read and converted. This option 
                                        should only be used for debugging 
                                        purposes to prevent conversion of large
                                        containers. It might not result in a 
                                        correctly converted logfile.
  --only arg                            Only convert the port given by this 
                                        argument.
  --start arg (=0)                      Index of the first sample that will be 
                                        exported. This option is only useful if
                                        only one stream will be exported.
  --end arg (=-1)                       Index after the last sample that will 
                                        be exported. This option is only useful
                                        if only one stream will be exported.
  --align_named_vector                  For named vector types, order the 
                                        elements by the names of the first 
                                        sample. Only one names vector valid for
                                        all samples will be included in the 
                                        output. Only works with --flatten.
  --flatten                             With --flatten, all nested field names 
                                        are joined into top level keys. This 
                                        makes the data fields directly 
                                        accessible.
  --compress                            Compress the output using msgpack's 
                                        zlib compression.

```

Example:

We read the logfile `test/data/laser_scan.0.log` and convert the only stream
`/message_producer.messages` to `laser_scan.msg`. Only samples between
indices 0 and 1 will be converted to the output file.

    $ pocolog2msgpack -l test/data/laser_scan.0.log -o laser_scan.msg --only /message_producer.messages --start 0 --end 1 --verbose 2
    [pocolog2msgpack] Verbosity level is 2.
    [pocolog2msgpack] Only converting port '/message_producer.messages'.
    Loading logfile test/data/laser_scan.0.log
    Stream /message_producer.messages [/base/samples/LaserScan]
      2 Samples from 20170727-15:17:28 to 20170727-15:17:29 [01:00:00:999]
    Stream /message_producer.state [/int32_t]
      2 Samples from 20170727-15:17:28 to 20170727-15:17:30 [01:00:02:716]
    Loading logfile Done test/data/laser_scan.0.log
    Loading logfile Done test/data/laser_scan.0.log
    Building multi file index 
     100% Done
    Processed 4 of 4 samples 
    [pocolog2msgpack] 1 streams
    [pocolog2msgpack] Stream #0 (/message_producer.messages): 2 samples
    [pocolog2msgpack] Converting sample #0

`pocolog2msgpack -v 1 --start=0 --end=1 --flatten --align_named_vector --compress -l *072_crex_ft*.log -l *072_crex_legs*.log  -o 20221013154136.072_data_obj.msg.zz`
    
## Without Installation

If you have docker and you do not want to install anything you can use our
image. Just go to the directory in which the log file is located and run

    docker run --interactive --rm --tty --volume "$PWD":/wd --workdir /wd -it af01/pocolog2msgpack [arguments ...]

It will create a container from our docker image, run pocolog2msgpack, and
destroy itself. The current directory is mounted within the temporary docker
container to be able to load files from the current directory. Output should
be written to the same folder. No complicated paths including `..` are allowed.
There is a slight runtime overhead for creating and destroying the docker
container. Since logfile conversions are not supposed to happen that often
it can be ignored though.

## Possible Errors

### Outdated Data Types (Segmentation Fault)

A logfile might contain an old version of a data type. Running pocolog2msgpack
will result in a segmentation fault when it tries to convert the outdated
sample. We have to preprocess the logfile to be able to convert it in this
case. See
[converting logfiles](https://www.rock-robotics.org/documentation/data_analysis/converting_logfiles.html)
for further instructions.

## Loading Logs in Python

Loading an uncompressed converted logfile in Python is as simple as those two lines:

```python
import msgpack
log = msgpack.unpack(open("output.msg", "rb"))
```

For a more advanced loading of log files, have a look at the Python package 
pocolog2msgpack.

Note 1: This loads the complete log file into memory at once.
Note 2: In older versions of msgpack, `encoding="utf8"` may be needed.

Make sure that msgpack is installed (e.g. `pip3 install msgpack`).
This is the only dependency. You do not have to install this package to load
the files once you converted them to MsgPack.

The object `log` is a Python dictionary that contains names of all logged ports
as keys and the logged data in a list as its keys.

The logdata itself is stored at the key `"/<task_name>.<port name>"` and
meta data is stored at `"/<task_name>.<port name>.meta"`. The meta data
contains the timestamp for each sample at the key `"timestamps"` and the
data type at the key `"type"`. At the moment, we use the type names from
Typelib.

If the `--flatten` option has been used to convert the log data, 
the keys will be of type `"/<task_name>.<port name>/<field name>/<sub-field name>...."`.
These keys map to a vector or an array of samples for this field, depending on the type the stream had originally.

There are several examples of how to use the tool and how to load data in
Python in the test folder. Further documentation can also be found in the Python pacakge pocolog2msgpack.

## Tests

We use nosetests to run the unit tests:

    $ nosetests3
    .................
    ----------------------------------------------------------------------
    Ran 17 tests in 2.074s

    OK

The packages nose and pexpect have to be installed with pip to run the tests.

## Contributing

Usually it is not possible to push directly to the master branch for anyone.
Only tiny changes, urgent bugfixes, and maintenance commits can be pushed
directly to the master branch by the maintainer without a review.
"Tiny" means backwards compatibility is mandatory and all tests must
succeed. No new feature must be added.

Developers have to submit pull requests. Those will be reviewed by at least
one other developer and merged by a maintainer. New features must be
documented and tested. Breaking changes must be discussed and announced
in advance with deprecation warnings.

## License

pocolog2msgpack is distributed under the
[LGPL-2.1 license](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html) or later.

Copyright 2017-2018 Alexander Fabisch, DFKI GmbH / Robotics Innovation Center
