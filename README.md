# unitree_sdk2

[![CI](https://github.com/jiangtao129/unitree_sdk_jt/actions/workflows/c-cpp.yml/badge.svg?branch=main)](https://github.com/jiangtao129/unitree_sdk_jt/actions/workflows/c-cpp.yml)

Unitree robot sdk version 2.

> This is a personal fork (`jiangtao129/unitree_sdk_jt`) of the upstream
> [unitreerobotics/unitree_sdk2](https://github.com/unitreerobotics/unitree_sdk2)
> with on-top work for Go2 EDU stair-climbing (`unitree_slam/example/src/keyDemo3.cpp`)
> and an agent-driven CI/PR pipeline. See `AGENTS.md` and `CONTRIBUTING.md`
> for fork-specific rules; everything else below is upstream.

### Prebuild environment
* OS  (Ubuntu 20.04 LTS)  
* CPU  (aarch64 and x86_64)   
* Compiler  (gcc version 9.4.0) 

### Environment Setup

Before building or running the SDK, ensure the following dependencies are installed:

- CMake (version 3.10 or higher)
- GCC (version 9.4.0)
- Make

You can install the required packages on Ubuntu 20.04 with:

```bash
apt-get update
apt-get install -y cmake g++ build-essential libyaml-cpp-dev libeigen3-dev libboost-all-dev libspdlog-dev libfmt-dev
```

### Build examples

To build the examples inside this repository:

```bash
mkdir build
cd build
cmake ..
make
```

### Installation

To build your own application with the SDK, you can install the unitree_sdk2 to your system directory:

```bash
mkdir build
cd build
cmake ..
sudo make install
```

Or install unitree_sdk2 to a specified directory:

```bash
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/unitree_robotics
sudo make install
```

You can refer to `example/cmake_sample` on how to import the unitree_sdk2 into your CMake project. 

Note that if you install the library to other places other than `/opt/unitree_robotics`, you need to make sure the path is added to "${CMAKE_PREFIX_PATH}" so that cmake can find it with "find_package()".

### Notice
For more reference information, please go to [Unitree Document Center](https://support.unitree.com/home/zh/developer).
