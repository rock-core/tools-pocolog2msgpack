.. _sec-usage:

=====
Usage
=====

1. Get available fields::


    from pocolog2msgpack import get_fields, read_field, read_fields

    path = "20221013154136.072_data.msg.zz"

    fields = get_fields(path)
    for f in fields:
        print(f)



2. Reading a single field::


    dat = {}

    dat["effort"] = read_field(path, "/crex_dispatcher_drv_meas.act_state_out/elements/effort")




3. Reading multiple fields and rename the target key::


    keys = {
        "/crex_foot_force_estimator.ff_estimated/elements/force/data" : "force_est",
        "/crex_dispatcher_ft_sensors.forces_all/elements/force/data" : "force_meas",
        "/crex_foot_force_estimator.ff_estimated/names" : "force_est_names",
        "/crex_dispatcher_ft_sensors.forces_all/names" : "force_meas_names",
        "/crex_foot_force_estimator.ff_estimated/time/microseconds" : "force_est_time",
        "/crex_dispatcher_ft_sensors.forces_all/time/microseconds" : "force_meas_time"
    }
    
    dat = read_fields(path, srcKeys=keys, renameMap=keys)




4.  Raw loading of complete log files in Python

    Loading an uncompressed converted logfile in Python is as simple as those two lines

    .. code-block:: python
    
        import msgpack
        log = msgpack.unpack(open("output.msg", "rb"))


    Note 1: This loads the complete log file into memory at once.
    Note 2: In older versions of msgpack, ``encoding="utf8"`` may be needed.

    Make sure that msgpack is installed (e.g. ``pip3 install msgpack``).
    This is the only dependency. You do not have to install this package to load
    the files once you converted them to MsgPack.

    The object `log` is a Python dictionary that contains names of all logged ports
    as keys and the logged data in a list as its keys.

    The logdata itself is stored at the key ``/<task_name>.<port name>`` and
    meta data is stored at ``/<task_name>.<port name>.meta``. The meta data
    contains the timestamp for each sample at the key `"timestamps"` and the
    data type at the key ``type``. At the moment, we use the type names from
    Typelib.

    If the ``--flatten`` option has been used to convert the log data, 
    the keys will be of type ``/<task_name>.<port name>/<field name>/<sub-field name>....``.
    These keys map to a vector or an array of samples for this field, depending on the type the stream had originally.


5.  Conversion to Relational Format

    To convert from data samples with nested fields to relational data, have a look at the function ``object2relational``.
    There is an optional Python package `pocolog2msgpack` that will be installed
    only if you have Python on your system. It can be used to convert data
    from object-oriented format to a relational format so that you can convert it
    directly to a [pandas.DataFrame](http://pandas.pydata.org/)
    
    .. code-block:: python

        import msgpack
        import pocolog2msgpack
        import pandas
        pocolog2msgpack.object2relational(logfile, logfile_relational)
        log = msgpack.unpack(open(logfile_relational, "rb"))
        df = pandas.DataFrame(log[port_name])
        # use the timestamp as an index:
        df.set_index("timestamp", inplace=True)
        # order columns alphabetically:
        df.reindex_axis(sorted(df.columns), axis=1, copy=False)


    Vectors or arrays will usually not be unravelled automatically even if they
    have the same size in each sample. You have to whitelist them manually, e.g. 
    
    .. code-block:: python

        pocolog2msgpack.object2relational(
            logfile, logfile_relational, whitelist=["elements", "names"])


    This will result in unravelled base::samples::Joints object in this case::


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
