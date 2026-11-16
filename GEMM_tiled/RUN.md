# p3 Tiled-GEMM — 실행 순서

저장소를 받은 뒤 `GEMM_tiled/` 안에서 실행한다.
(설정은 p2와 같다: PYNQ-Z2 `xc7z020clg400-1`, 100 MHz, Vitis HLS 2025.1)

## 0. C simulation만 먼저 (툴 없이, 10초)

```bash
cd GEMM_tiled
g++ -O0 -Wno-unknown-pragmas -o csim gemm_tiled.cc gemm_tiled_tb.cc && ./csim
```
4줄 PASS + `success test bench` 가 나와야 한다.

## 1. 최적화 버전 (UNROLL 8 + PARTITION complete)

```bash
source /tools/2025.1/Vitis/settings64.sh                                 # 설치 경로에 맞게
cd GEMM_tiled
v++ -c --mode hls --config hls_config.cfg --work_dir opt                 # C 합성
vitis-run --mode hls --cosim --config hls_config.cfg --work_dir opt      # co-sim
```
리포트:
- `opt/hls/syn/report/csynth.rpt`  → 전체 latency(cycles) · 루프별 II · BRAM/DSP/FF/LUT
- `opt/hls/sim/report/gemm_tiled_cosim.rpt` → Total Execution Time (co-sim)
- co-sim 로그에 PASS 4줄이 찍히는지 확인 (`opt/hls/sim/report/` 또는 터미널)

## 2. baseline 버전 (지시문 없이 맨 안쪽 PIPELINE만) — 비교용

```bash
v++ -c --mode hls --config hls_config_baseline.cfg --work_dir base
vitis-run --mode hls --cosim --config hls_config_baseline.cfg --work_dir base
```
`syn.cflags=-DBASELINE` 이 안 먹으면(에러가 나면) `gemm_tiled.cc` 맨 위에
`#define BASELINE` 한 줄을 넣고 `hls_config.cfg` 로 `--work_dir base` 를 돌린다.

다시 합성하면 이전 co-sim 리포트가 지워진다. work_dir 를 `opt` · `base` 로 나눠 뒀으니 서로 겹치지 않는다.

## 3. GUI로 할 때
Vitis Unified IDE → Open Component → 이 폴더(`vitis-comp.json`) → C Simulation → C Synthesis → Co-simulation.
Synthesis Summary · Co-simulation Report 화면을 캡처해 둔다 (보고서 그림용).

## 4. 보고서에 쓸 리포트
- `opt/hls/syn/report/csynth.rpt`
- `opt/hls/sim/report/gemm_tiled_cosim.rpt`
- `base/hls/syn/report/csynth.rpt`
- `base/hls/sim/report/gemm_tiled_cosim.rpt`
- 캡처 PNG 2~3장 (Synthesis Summary, Co-sim Report)

## 5. 제출 zip (저장소 루트에서)
```bash
zip -r <학번>_<이름>_code.zip GEMM_tiled -x "GEMM_tiled/opt/*" "GEMM_tiled/base/*" "GEMM_tiled/csim" "GEMM_tiled/GEMM_tiled/*" "GEMM_tiled/_ide/*"
```
(소스 3개 + cfg 2개 + vitis-comp.json + RUN.md 만 들어가면 된다)
