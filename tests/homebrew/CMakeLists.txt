# The test programs, built with devkitPPC and wut (waterbox/build-homebrew.sh).
# They are this repository's own, made for the tests: no game is needed to run
# them.
cmake_minimum_required(VERSION 3.13)
project(chimera-cemu-homebrew C)

add_executable(frames frames/main.c)
target_compile_options(frames PRIVATE -O2 -Wall)
wut_create_rpx(frames)

add_executable(pad pad/main.c)
target_compile_options(pad PRIVATE -O2 -Wall)
wut_create_rpx(pad)

add_executable(fonts fonts/main.c)
target_compile_options(fonts PRIVATE -O2 -Wall)
wut_create_rpx(fonts)

# not "gx2": a program is a module named after its file, and one named gx2
# would be taken for the console's library of that name
add_executable(draw draw/main.c)
target_compile_options(draw PRIVATE -O2 -Wall)
wut_create_rpx(draw)

add_executable(saves saves/main.c)
target_compile_options(saves PRIVATE -O2 -Wall)
wut_create_rpx(saves)

add_executable(readback readback/main.c)
target_compile_options(readback PRIVATE -O2 -Wall)
wut_create_rpx(readback)

add_executable(cores cores/main.c)
target_compile_options(cores PRIVATE -O2 -Wall)
wut_create_rpx(cores)

add_executable(mii mii/main.c)
target_compile_options(mii PRIVATE -O2 -Wall)
wut_create_rpx(mii)

add_executable(storage storage/main.c)
target_compile_options(storage PRIVATE -O2 -Wall)
wut_create_rpx(storage)
