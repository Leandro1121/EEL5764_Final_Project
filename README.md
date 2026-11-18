# EEL5764_Final_Project

## Setting up the programs 

```bash
sudo apt install gcc-riscv64-linux-gnu
cd program
cmake -B build-rv -DCMAKE_C_COMPILER=riscv64-linux-gnu-gcc
cmake --build build-rv
file build-rv/bin/*      # expect: ELF 64-bit LSB executable, UCB RISC-V ... statically linked
```

## Running one simulation
```bash
cd ~/gem5
build/RISCV/gem5.opt -d m5out/gather_fill2 configs/deprecated/example/se.py \
  --cpu-type=RiscvO3CPU --caches --l2cache --cpu-clock=3GHz \
  --mem-type=SimpleMemory --mem-size=2GiB \
  --warmup-insts=10000000 --maxinsts=30000000 \
  --cmd=path/to/build-rv/bin/gather \
  '--options=-mb 16 -fill 2 -iters 2000000'
```