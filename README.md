# DDR Model

## Directory Tree
```text
|-- CHIPort
|   |-- include
|   |   |__ CHIPort
|   |__ src
|-- DMU
|   |-- include
|   |   |__ DMU
|   |__ src
|-- lib
    |-- amba_tlm
    |   |-- include
    |   |   |__ ARM
    |   |__ src
    |-- DRAMSys
        |-- cmake
        |-- configs
        |   |-- addressmapping
        |   |-- mcconfig
        |   |-- memspec
        |   |-- simconfig
        |   |__ traces
        |-- lib
        |   |-- nlohmann_json
        |   |__ sqlite3
        |__ src
            |-- configuration
            |-- libdramsys
            |-- simulator
            |__ util
```
## LIB Include Directories

### CHIPort
```makefile
-I ../CHIPort/include
```
### DMU
```Makefile
-I ../DMU/include
```
### AMNA TLM 库
```Makefile
AMBA TLM 库
```
### DRAMSys 下的各依赖库
```Makefile
-I ../lib/DRAMSys/lib/nlohmann_json/include
-I ../lib/DRAMSys/lib/sqlite3/sqlite

-I ../lib/DRAMSys/src/configuration
-I ../lib/DRAMSys/src/libdramsys
-I ../lib/DRAMSys/src/simulator
-I ../lib/DRAMSys/src/util
```
### 标准文件库
由于gcc8.3.1的标准库中并没有集成文件系统 std::filesystem，要到gcc9之上才能支持，所以在编译链接时需要额外指定标准库stdc++fs
```Makefile
-lstdc++fs
```
### 需要指定DRAMSys的配置文件目录，使用宏指定；
```Makefile
#配置文件目录在"${home}/.../lib/DRAMSys/configs"
DRAMSYS_RESOURCE_DIR = "你的路径"
#例如"/cloud/home/liujungan1756/software/DDR_TLM/lib/DRAMSys/configs"
#必须要指定，否则DRAMSys无法找到配置文件，会报错
#宏约定义
START_FROM_DDR
#在../DRAMSys/src/libdramsys/DRAMSys/simulation/dram/Dram.cpp中执行读取.hex文件
START_OS
#在../DRAMSys/src/libdramsys/DRAMSys/simulation/dram/Dram.cpp中执行读取.hex文件
OS_FILE_NAME
#确定被读取的os相关的.hex文件
HEX_FILE_NAME
#确定被读取的uboot相关的.hex文件
OS_HEX_ADDR #.hex文件被读取的位置
UBOOT_HEX_ADDR #.hex文件被读取的位置
DDR_BASE_ADDR #DDR的基地址

```
### 代码集成说明
```
DDR的模型由两部分组成，自写CHIPort和DRAMSys，此二者已经进行封装在一起，可以只用DMU中的“DramMannageUnit”类进行调用； 样例实现，可以参照DMU目录下的"main.cc"文件进行；
```

## Example运行
```shell
# using module load the cmake and gcc
# gcc9以上
#在DDR-MODEL目录下
mkdir build
cd build 
cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..
make -j$(nproc)
export LD_LIBRARY_PATH=/opt/systemc-2/lib:$LD_LIBRARY_PATH
cd bin
./dmutest
```