#! /usr/bin/env python

from distutils.core import setup

def setup_package():
    setup(
        name="pocolog2msgpack",
        version="1.0.0",
        author="Alexander Fabisch",
        author_email="alexander.fabisch@dfki.de",
        description="Python tools for pocolog2msgpack.",
        long_description=open("README.md").read(),
        license="unknown",
        py_modules=["pocolog2msgpack"],
        requires=["msgpack"],
        scripts=["bin/rock2infuse"],
    )


if __name__ == "__main__":
    setup_package()
