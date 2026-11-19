# AI Accelerator HLS — VADD · GEMM on PYNQ-Z2

인공지능가속기설계 실습 과제 **p1 VADD**, **p2 GEMM**, **p3 Tiled-GEMM**의 Vitis HLS 소스, 합성(C Synthesis) 리포트, C/RTL co-simulation 리포트입니다.

- 툴: **Vitis HLS 2025.1** (Vivado IP Flow Target)
- 보드: **PYNQ-Z2** (`xc7z020clg400-1`), 목표 클럭 **10 ns (100 MHz)**
- Available: BRAM_18K 280 · DSP 220 · FF 106,400 · LUT 53,200

## 구조

```
vadd/                vadd.cc · vadd.h · vadd_tb.cc · hls_config.cfg · vitis-comp.json
GEMM/                gemm.cc (Opt 1) · gemm.h · gemm_tb.cc · hls_config.cfg · vitis-comp.json
GEMM_tiled/          gemm_tiled.cc · gemm_tiled.h · gemm_tiled_tb.cc   (IDE 컴포넌트 Gemm_Tiled가 이 소스를 씀)
results/vadd/        csynth.rpt · vadd_csynth.rpt · vadd_cosim.rpt
results/gemm_opt1/   csynth.rpt · gemm_csynth.rpt · gemm_cosim.rpt
results/p3_final_struct_u16/   csynth.rpt · gemm_tiled_csynth.rpt · gemm_tiled_cosim.rpt · cosim_transaction.rpt
```

## 실행

Vitis Unified IDE에서 `vadd` 또는 `GEMM` 컴포넌트를 열고 C Simulation → C Synthesis → C/RTL Co-simulation 순서로 실행합니다. CLI로는:

```bash
source <Vitis 설치 경로>/settings64.sh
cd GEMM
v++ -c --mode hls --config hls_config.cfg --work_dir GEMM          # C Synthesis
vitis-run --mode hls --cosim --config hls_config.cfg --work_dir GEMM   # C/RTL Co-simulation
```

## p1 VADD

길이 1,024 `float` 벡터 덧셈. `memcpy`로 on-chip 버퍼에 복사 → 덧셈 루프 → 결과 되쓰기. 인터페이스 지시문만 사용.

| 합성 latency | co-sim (Total Execution Time) | BRAM | DSP | FF | LUT |
|---|---|---|---|---|---|
| 3,105 | **3,120** (Pass) | 10 | 2 | 3,001 | 3,530 |

## p2 GEMM — directive 비교

A(10×64) × B(64×64) = C(10×64), `float`. 행렬곱 루프 순서는 **m → n → o**(B는 transpose하지 않음)이고, 맨 안쪽 루프 라벨 `idx_n`이 실제로는 **o 루프**입니다. 아래 설정은 `gemm.cc`의 directive만 바꿔 합성했습니다. 리포트 파일은 Opt 1만 포함합니다.

| 설정 | directive | 행렬곱 II | 합성 latency | co-sim | BRAM | DSP | FF | LUT |
|---|---|---|---|---|---|---|---|---|
| Baseline | 없음 (맨 안쪽 `PIPELINE` 한 줄도 결과 동일) | 1 | 45,737 | 45,752 | 16 | 5 | 3,419 | 4,572 |
| **Opt 1** | B·C `ARRAY_PARTITION type=cyclic factor=8 dim=2` + `idx_n` 안 `UNROLL factor=8` | 1 | **9,896** | **9,911** | 30 | 40 | 7,272 | 10,541 |
| Opt 2 | Opt 1 + `idx_n` 안 `PIPELINE II=4` | 4 | 25,255 | 25,270 | 24 | 10 | 4,693 | 6,356 |
| 비교 A | 읽기·초기화 루프 `UNROLL factor=64` + A·B `complete dim=2` | 6 | 250,531 | — | 68 | 5 | 18,179 | 19,372 |
| 비교 B | `idx_n` 안 `UNROLL factor=32` (파티션 지시 없음) | 6 | 12,456 | — | 70 | 30 | 19,066 | 22,415 |
| 비교 C | Opt 1 + `wrcdata_c`(n 루프) 안 `PIPELINE II=1` | 8 | 9,899 | — | 38 | 40 | 15,282 | 13,776 |
| 비교 D | Opt 1 + `idx_n` 안 `PIPELINE II=8` | 8 | 45,735 | — | 23 | 5 | 4,129 | 5,553 |

- **Opt 1**: o 루프를 8개씩 펼쳐 MAC 8개(DSP 40)를 두고, 펼친 8개가 서로 다른 bank에 있도록 B·C를 cyclic 8로 나눠 II 1을 유지했습니다. co-sim 기준 baseline 대비 4.6배.
- `GEMM/gemm.cc`(Opt 1)의 `idx_n` 안에는 II를 생략한 `#pragma HLS PIPELINE` 한 줄도 있습니다. 기본 II가 1이라 툴의 자동 pipeline과 결과가 같아서, 이 줄이 있든 없든 합성 9,896 · co-sim 9,911로 동일합니다(`results/gemm_opt1/`은 이 줄이 있는 상태의 리포트).
- **II와 자원**: 같은 UNROLL 8에서 II 1 · 4 · 8 → DSP 40 · 10 · 5. 처리량 ≈ factor ÷ II.
- **비교 A~D**: 32비트 m_axi 포트 한계(읽기 루프 unroll 무효), C 자동 분할 후 누산 의존(II 6), 바깥 루프 PIPELINE 시 안쪽 루프 전체 unroll(II 8)을 보여 줍니다.
- Opt 1에서도 데이터 이동(B 읽기 + C 쓰기)이 전체의 약 48%라, 다음 단계는 포트 폭 확장이나 타일링입니다.

## p3 Tiled-GEMM

A(M×N) × B(N×O) = C(M×O), `float`. **M · N · O는 실행 중에 s_axilite 레지스터**(`M` 0x34 · `N` 0x3c · `O` 0x44)로 받고, 타일 크기는 헤더 상수 `T_M`=5 · `T_N`=32 · `T_O`=32입니다. C 타일을 고정하고 k 방향으로 A·B 타일을 옮기며 누적하는 output-stationary 타일링이고, 경계 타일은 범위 밖을 0으로 채워 읽고 범위 안 칸만 씁니다.

- 연산: `compute_tile`의 nn → mm → oo 루프, 맨 안쪽 oo에 `PIPELINE` + `UNROLL factor=16`, B·C 버퍼 `ARRAY_PARTITION type=cyclic factor=16 dim=2` (MAC 16개, II 1)
- 구조: ① ping-pong 버퍼(계산하는 동안 다음 타일 읽기) ② 타일 3중 루프를 `step` 루프 하나로 합침 ③ C 초기화 루프 제거(k 첫 타일에서 0부터 누적) ④ 꽉 찬 타일은 조건 없는 루프로 읽어 burst(길이 32) 추론
- 테스트벤치: 같은 하드웨어로 10×64×64(P2 크기) · 16×32×24 · 13×26×18 · 3×5×7 · 10×40×33 실행. 뒤의 넷은 타일 크기로 나누어떨어지지 않음. golden 비교 + M×O 밖 overrun 검사, C sim · co-sim 모두 PASS

| 단계 (타일 5×32×32) | co-sim (10×64×64) | BRAM | DSP | FF | LUT |
|---|---|---|---|---|---|
| directive 없음 | 54,887 | 9 | 17 | 7,059 | 6,732 |
| `PIPELINE` + `UNROLL 4` (B·C `PARTITION 8`) | 27,387 | 13 | 32 | 9,933 | 10,375 |
| `UNROLL 8` + `PARTITION 8` | 19,043 | 13 | 52 | 11,720 | 12,948 |
| + 구조 개선 ①~④ | 11,549 | 22 | 70 | 14,331 | 16,416 |
| **+ `UNROLL 16` + `PARTITION 16` (최종)** | **11,229** | 38 (13%) | 110 (50%) | 19,183 (18%) | 24,666 (46%) |
| 최종에서 `PARTITION 32`만 변경 | 23,520 (II 8) | 70 | 40 | 12,660 | 18,710 |

- 최종 모듈(합성): load_a 177 · load_b 1,041 · compute_tile 334 · store 172 cycle, 추정 클럭 7.30 ns. load_a · load_b · compute_tile이 동시에 시작하므로 step 1회는 B 타일 읽기(1,041)가 정하고, 연산은 그 안에 숨습니다.
- **P2 Opt 1(9,911)과 비교: 1.13배 느림.** T_M = 5 < M = 10이라 B를 두 번(8,192원소) 읽고 32비트 포트는 cycle당 1원소라, 연산을 두 배로 늘려도 메모리 읽기가 시간을 정합니다(구조 개선 뒤 UNROLL 8 → 16은 DSP +40에 latency −3%). 대신 행렬 크기를 재합성 없이 바꿀 수 있고 on-chip 메모리가 타일 크기로 고정됩니다.
- 합성 리포트의 전체 latency는 `?`입니다(M · N · O를 실행 중에 받아 반복 횟수를 모름). latency는 co-sim으로 잽니다. co-sim TOTAL은 TB 케이스 합이라 `cosim_transaction.rpt`의 transaction 0(10×64×64)을 봅니다.
- IDE 컴포넌트 `Gemm_Tiled`의 `hls_config.cfg` (빌드 폴더는 올리지 않음):

```ini
part=xc7z020clg400-1

[hls]
flow_target=vivado
package.output.format=ip_catalog
package.output.syn=false
syn.file=../GEMM_tiled/gemm_tiled.cc
syn.file=../GEMM_tiled/gemm_tiled.h
syn.top=gemm_tiled
tb.file=../GEMM_tiled/gemm_tiled_tb.cc
clock=100mhz
```
