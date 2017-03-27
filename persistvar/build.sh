#!/bin/bash
# import the environment
source env.sh

# clean up befor build
. clean.sh

# STEP 1. pull Matt's serialization/deserialization library
mkdir ${DEPENDENCIES}
cd ${DEPENDENCIES}
git clone https://github.com/mpmilano/mutils-serialization.git
assert_success "git clone mutils-serialization"
cd mutils-serialization
git checkout 1b5939e29176b2c78c9df4aa14d8f1c825a8586e
assert_success "git checkout 1b5939e29176b2c78c9df4aa14d8f1c825a8586e"
cd ..
git clone https://github.com/mpmilano/mutils.git
assert_success "git clone mutils"
cd mutils
git checkout  fe36060bce7624ff1cf7451db915865d16d1628d
assert_success "git checkout  fe36060bce7624ff1cf7451db915865d16d1628d"
cd ..
 
# STEP 2. build Matt's library, put the include files in place
cd mutils
make
assert_success "making mutils dependency"
cd ../mutils-serialization
mv Makefile Makefile.tmp
cat Makefile.tmp | sed '/^CPPFLAGS/s/$/ -I..\/mutils/' | sed '/^LDFLAGS/s/$/ -L..\/mutils/' > Makefile
make
assert_success "making mutils-serialization dependency"
cd ..

# STEP 3. pull other dependencies
git clone https://github.com/gabime/spdlog.git
assert_success "git clone https://github.com/gabime/spdlog.git"
cd spdlog
git checkout 029e6ed40fdbca23c4804189254d226190181b73
assert_success "git checkout 029e6ed40fdbca23c4804189254d226190181b73"
cd ..

# leave dependencies
cd ..
# STEP 4. build this library
echo "mkdir ${BUILD}"
mkdir ${BUILD}
echo "done mkdir ${BUILD}"
cd ${BUILD}
cmake ..
assert_success "cmaking persistvar"
make
assert_success "making persistvar"
cd ..
