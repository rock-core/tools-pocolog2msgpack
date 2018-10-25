source /opt/rock/env.sh
mkdir build
cd build
cmake ..
make install
cd ..
python setup.py install
