#!/usr/bin/bash

# Extract a subset of the files from the installation directory (--prefix)
# to create a minimal Derivgrind installation. 
# The valgrind executable is renamed into derivgrind-valgrind to avoid
# shadowing the user's Valgrind installation.

original_install=$1
exported_install=$1.export

rm -rf $exported_install
mkdir -p $exported_install/bin
cp $original_install/bin/valgrind $exported_install/bin/derivgrind-valgrind
cp $original_install/bin/derivgrind-config $exported_install/bin/derivgrind-config
cp $original_install/bin/derivgrind $exported_install/bin/derivgrind
cp $original_install/bin/tape-evaluation $exported_install/bin/tape-evaluation

mkdir -p $exported_install/libexec/valgrind
for file in derivgrind-amd64-linux derivgrind-x86-linux vgpreload_core-amd64-linux.so vgpreload_core-x86-linux.so vgpreload_derivgrind-amd64-linux.so vgpreload_derivgrind-x86-linux.so; do
  cp $original_install/libexec/valgrind/$file $exported_install/libexec/valgrind/$file
done;

mkdir -p $exported_install/include/valgrind
for file in valgrind.h derivgrind.h derivgrind-recording.h; do
  cp $original_install/include/valgrind/$file $exported_install/include/valgrind/$file
done
