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
