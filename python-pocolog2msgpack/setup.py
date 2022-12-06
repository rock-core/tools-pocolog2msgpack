#!/usr/bin/env python

"""The setup script."""

from setuptools import setup, find_packages

with open('README.rst') as readme_file:
    readme = readme_file.read()

with open('HISTORY.rst') as history_file:
    history = history_file.read()

requirements = [ "msgpack>=1.0.3", "numpy" ]

test_requirements = [ ]

setup(
    author="Vinzenz Bargsten, Alexander Fabisch",
    author_email='vinzenz.bargsten@dfki.de',
    python_requires='>=3.6',
    classifiers=[
        'Development Status :: 2 - Pre-Alpha',
        'Intended Audience :: Developers',
        'Natural Language :: English',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
    ],
    description="Helper modules to work with MessagePack files generated from pocolog log files",
    install_requires=requirements,
    long_description=readme + '\n\n' + history,
    include_package_data=True,
    keywords='pocolog2msgpack',
    license="GNU LESSER GENERAL PUBLIC LICENSE",
    name='pocolog2msgpack',
    packages=find_packages(include=['pocolog2msgpack', 'pocolog2msgpack.*']),
    test_suite='tests',
    tests_require=test_requirements,
    url='https://github.com/rock-core/tools-pocolog2msgpack',
    version='1.1.0',
    zip_safe=False,
)
