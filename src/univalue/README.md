
# UniValue

## Summary

A universal value class, with JSON encoding and decoding.

UniValue is an abstract data type that may be a null, boolean, string,
number, array container, or a key/value dictionary container, nested to
an arbitrary depth.

This class is aligned with the JSON standard, [RFC
7159](https:

## Installation

This project is a standard GNU
[autotools](https:
project.  Build and install instructions are available in the `INSTALL`
file provided with GNU autotools.

```
$ ./autogen.sh
$ ./configure
$ make
```

## Design

UniValue provides a single dynamic RAII C++ object class,
and minimizes template use (contra json_spirit).

