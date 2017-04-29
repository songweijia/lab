#!/bin/bash
sudo pip uninstall cepheval -y
python setup.py build
sudo python setup.py install
