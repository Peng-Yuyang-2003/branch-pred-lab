# 第二次实验：gem5 分支预测实验说明

本文件只保留完成实验需要的步骤。默认当前目录是 `<gem5根目录>`，并且已经存在：

```text
./build/X86/gem5.opt
```

## 1. 实验前提

### 1.1 下载课程代码

把课程 GitHub 仓库下载到 gem5 的 `configs/branch_pred_lab/` 目录：

```bash
cd <gem5根目录>/configs
git clone https://github.com/K1ssMe520/thu_gem5_experimental_2026.git
cp -r thu_gem5_experimental_2026/第二次实验_CPU搭建与分支预测 ./branch_pred_lab
cd <gem5根目录>
```

确认文件存在：

```bash
ls configs/branch_pred_lab
```

应至少看到：

```text
branchy_benchmark.cpp
extract_stats.py
README.md
TEACHING.md
LAB_INSTRUCTIONS.md
第二次实验报告模板.docx
第二次实验结果空表.csv
```

### 1.2 本实验需要提交

- 填写后的 `第二次实验报告模板.docx`
- 填写后的 `第二次实验结果空表.csv`
- 自己创建的 `configs/branch_pred_lab/o3_bp_config.py`
- 必要截图：编译、trace 输出、gem5 运行、`extract_stats.py` 输出

## 2. 初步搭建 CPU

### 2.1 创建 hello 程序

创建：

```text
configs/branch_pred_lab/hello_student.cpp
```

写入：

```cpp
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    std::string student_id = argc > 1 ? argv[1] : "unknown_id";
    std::string name = argc > 2 ? argv[2] : "unknown_name";
    std::cout << "Hello gem5\n";
    std::cout << "student_id=" << student_id << "\n";
    std::cout << "name=" << name << "\n";
    return 0;
}
```

编译：

```bash
/usr/bin/g++ -O2 -std=c++17 -march=x86-64 -mtune=generic \
  configs/branch_pred_lab/hello_student.cpp \
  -o configs/branch_pred_lab/hello_student
```

本机运行，把参数换成自己的学号姓名：

```bash
configs/branch_pred_lab/hello_student 20240001 ZhangSan
```

保留截图：编译命令、本机运行输出。

### 2.2 创建 `o3_bp_config.py`

创建文件：

```text
configs/branch_pred_lab/o3_bp_config.py
```

按 `TEACHING.md` 第 2.4 节把各段代码依次拼接成初版配置文件。完成后检查语法：

```bash
python3 -m py_compile configs/branch_pred_lab/o3_bp_config.py
```

运行 hello：

```bash
./build/X86/gem5.opt \
  -d configs/branch_pred_lab/results/hello \
  configs/branch_pred_lab/o3_bp_config.py \
  --binary configs/branch_pred_lab/hello_student \
  --program-args "20240001 ZhangSan"
```

检查输出：

```bash
cat configs/branch_pred_lab/results/hello/simout
python3 configs/branch_pred_lab/extract_stats.py configs/branch_pred_lab/results/hello/stats.txt
```

保留截图：gem5 运行命令、`simout`、统计脚本输出。

## 3. 探索 Local 分支预测器

### 3.1 编译 benchmark

```bash
/usr/bin/g++ -O2 -std=c++17 -march=x86-64 -mtune=generic \
  configs/branch_pred_lab/branchy_benchmark.cpp \
  -o configs/branch_pred_lab/branchy_benchmark
```

### 3.2 观察稳定偏置分支模式

```bash
configs/branch_pred_lab/branchy_benchmark --iterations 13 --scenario predictable --period 32 --trace
```

`--trace` 会固定输出前 100 个动态分支结果，并按 `branch0` 到 `branch7` 各占一行。保留本次输出截图。

### 3.3 修改 `o3_bp_config.py` 支持 `none/local`

按 `TEACHING.md` 第 3.4 节修改：

- 在 `parse_args()` 中加入 `--bp-type`、`--local-size`、`--ctr-bits`
- 添加 `build_branch_predictor(args)`
- 在 `system.cpu = X86O3CPU()` 后加入 `system.cpu.branchPred = build_branch_predictor(args)`

检查语法：

```bash
python3 -m py_compile configs/branch_pred_lab/o3_bp_config.py
```

### 3.4 运行 none/local 对比

```bash
./build/X86/gem5.opt -d configs/branch_pred_lab/results/none_predictable \
  configs/branch_pred_lab/o3_bp_config.py \
  --binary configs/branch_pred_lab/branchy_benchmark \
  --bp-type none \
  --program-args "--iterations 50000 --working-set 4096 --seed 1 --scenario predictable --period 32 --history-len 96"

./build/X86/gem5.opt -d configs/branch_pred_lab/results/local_predictable \
  configs/branch_pred_lab/o3_bp_config.py \
  --binary configs/branch_pred_lab/branchy_benchmark \
  --bp-type local \
  --program-args "--iterations 50000 --working-set 4096 --seed 1 --scenario predictable --period 32 --history-len 96"
```

提取结果：

```bash
python3 configs/branch_pred_lab/extract_stats.py configs/branch_pred_lab/results/none_predictable/stats.txt
python3 configs/branch_pred_lab/extract_stats.py configs/branch_pred_lab/results/local_predictable/stats.txt
```

把结果填入 CSV，保留截图。

## 4. 探索高级分支预测器

### 4.1 观察 LocalBP 三种弱点分支模式

```bash
configs/branch_pred_lab/branchy_benchmark --iterations 13 --scenario tournament --period 8 --trace
configs/branch_pred_lab/branchy_benchmark --iterations 13 --scenario bimode --period 1 --trace
configs/branch_pred_lab/branchy_benchmark --iterations 13 --scenario tage --period 128 --history-len 96 --trace
```

保留三次 trace 输出截图。

### 4.2 运行 LocalBP 三种弱点场景

```bash
./build/X86/gem5.opt -d configs/branch_pred_lab/results/local_tournament \
  configs/branch_pred_lab/o3_bp_config.py \
  --binary configs/branch_pred_lab/branchy_benchmark \
  --bp-type local \
  --program-args "--iterations 50000 --working-set 4096 --seed 1 --scenario tournament --period 8 --history-len 96"

./build/X86/gem5.opt -d configs/branch_pred_lab/results/local_bimode \
  configs/branch_pred_lab/o3_bp_config.py \
  --binary configs/branch_pred_lab/branchy_benchmark \
  --bp-type local \
  --program-args "--iterations 50000 --working-set 4096 --seed 1 --scenario bimode --period 1 --history-len 96"

./build/X86/gem5.opt -d configs/branch_pred_lab/results/local_tage \
  configs/branch_pred_lab/o3_bp_config.py \
  --binary configs/branch_pred_lab/branchy_benchmark \
  --bp-type local \
  --program-args "--iterations 50000 --working-set 4096 --seed 1 --scenario tage --period 128 --history-len 96"
```

提取结果：

```bash
python3 configs/branch_pred_lab/extract_stats.py configs/branch_pred_lab/results/local_tournament/stats.txt
python3 configs/branch_pred_lab/extract_stats.py configs/branch_pred_lab/results/local_bimode/stats.txt
python3 configs/branch_pred_lab/extract_stats.py configs/branch_pred_lab/results/local_tage/stats.txt
```

填入 CSV，保留截图。

### 4.3 修改配置文件支持高级预测器

按 `TEACHING.md` 第 4.1 节修改 `o3_bp_config.py`，支持：

```text
none, local, tournament, bimode, tage
```

检查语法：

```bash
python3 -m py_compile configs/branch_pred_lab/o3_bp_config.py
```

### 4.4 运行高级预测器对比

TournamentBP：

```bash
./build/X86/gem5.opt -d configs/branch_pred_lab/results/tournament_tournament \
  configs/branch_pred_lab/o3_bp_config.py \
  --binary configs/branch_pred_lab/branchy_benchmark \
  --bp-type tournament \
  --program-args "--iterations 50000 --working-set 4096 --seed 1 --scenario tournament --period 8 --history-len 96"
```

BiModeBP：

```bash
./build/X86/gem5.opt -d configs/branch_pred_lab/results/bimode_bimode \
  configs/branch_pred_lab/o3_bp_config.py \
  --binary configs/branch_pred_lab/branchy_benchmark \
  --bp-type bimode \
  --program-args "--iterations 50000 --working-set 4096 --seed 1 --scenario bimode --period 1 --history-len 96"
```

TAGE：

```bash
./build/X86/gem5.opt -d configs/branch_pred_lab/results/tage_tage \
  configs/branch_pred_lab/o3_bp_config.py \
  --binary configs/branch_pred_lab/branchy_benchmark \
  --bp-type tage \
  --program-args "--iterations 50000 --working-set 4096 --seed 1 --scenario tage --period 128 --history-len 96"
```

提取结果：

```bash
python3 configs/branch_pred_lab/extract_stats.py configs/branch_pred_lab/results/tournament_tournament/stats.txt
python3 configs/branch_pred_lab/extract_stats.py configs/branch_pred_lab/results/bimode_bimode/stats.txt
python3 configs/branch_pred_lab/extract_stats.py configs/branch_pred_lab/results/tage_tage/stats.txt
```

### 4.5 最终提交前检查

CSV 至少应包含 8 行结果：

1. `predictable + none`
2. `predictable + local`
3. `tournament + local`
4. `tournament + tournament`
5. `bimode + local`
6. `bimode + bimode`
7. `tage + local`
8. `tage + tage`

报告中至少包含：

- hello 程序运行截图
- 四种 trace 输出截图
- 8 次 gem5 结果截图或 `extract_stats.py` 输出截图
- CSV 截图或附件
- 对四组对比的解释：none->local、local->tournament、local->bimode、local->tage
