#!/usr/bin/python

from setuptools import setup

setup(name='cepheval',
  version='0.1',
  description='ceph evaluation project',
  url='',
  author='Weijia Song',
  author_email='songweijia@gmail.com',
  license='NEW BSD LICENSE',
  packages=['cepheval'],
  package_dir={'cepheval':'cepheval'},
  install_requires=[
    'paramiko',
    'numpy',
    'scipy',
    'matplotlib',
  ],
  zip_safe=False)
