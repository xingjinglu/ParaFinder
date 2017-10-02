cmake -DCMAKE_BUILD_TYPE="Debug" -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc  -DCMAKE_INSTALL_PREFIX=/home/lxj/software/llvmsvn -DCMAKE_EXPORT_COMPILE_COMMANDS=ON /home/lxj/software/llvm-3.2-svn

make -j32
make install -j32
