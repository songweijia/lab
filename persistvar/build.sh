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
git clone https://github.com/mpmilano/mutils.git
assert_success "git clone mutils"
 
# STEP 2. build Matt's library, put the include files in place
cd mutils
make
assert_success "making mutils dependency"
cd ../mutils-serialization
mv Makefile Makefile.tmp
cat Makefile.tmp | sed '/^CPPFLAGS/s/$/ -I..\/mutils/' | sed '/^LDFLAGS/s/$/ -L..\/mutils/' > Makefile
make
assert_success "making mutils-serialization dependency"
cd ../..

# STEP 3. build this library
echo "mkdir ${BUILD}"
mkdir ${BUILD}
echo "done mkdir ${BUILD}"
cd ${BUILD}
cmake ..
assert_success "cmaking persistvar"
make
assert_success "making persistvar"
cd ..
