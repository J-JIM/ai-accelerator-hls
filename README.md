# AI Accelerator HLS — VADD · GEMM on PYNQ-Z2

인공지능가속기설계 실습 과제 **p1 VADD**와 **p2 GEMM**의 Vitis HLS 소스, 합성(C Synthesis) 리포트, C/RTL co-simulation 리포트입니다.

- 툴: **Vitis HLS 2025.1** (Vivado IP Flow Target)
- 보드: **PYNQ-Z2** (`xc7z020clg400-1`), 목표 클럭 **10 ns (100 MHz)**
- Available: BRAM_18K 280 · DSP 220 · FF 106,400 · LUT 53,200

## 구조

```
vadd/                vadd.cc · vadd.h · vadd_tb.cc · hls_config.cfg · vitis-comp.json
GEMM/                gemm.cc (Opt 1) · gemm.h · gemm_tb.cc · hls_config.cfg · vitis-comp.json
results/vadd/        csynth.rpt · vadd_csynth.rpt · vadd_cosim.rpt
results/gemm_opt1/   csynth.rpt · gemm_csynth.rpt · gemm_cosim.rpt
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
- **II와 자원**: 같은 UNROLL 8에서 II 1 · 4 · 8 → DSP 40 · 10 · 5. 처리량 ≈ factor ÷ II.
- **비교 A~D**: 32비트 m_axi 포트 한계(읽기 루프 unroll 무효), C 자동 분할 후 누산 의존(II 6), 바깥 루프 PIPELINE 시 안쪽 루프 전체 unroll(II 8)을 보여 줍니다.
- Opt 1에서도 데이터 이동(B 읽기 + C 쓰기)이 전체의 약 48%라, 다음 단계는 포트 폭 확장이나 타일링입니다.
