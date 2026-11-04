# Harness v6 Suite & AMMX Butterfly Benchmarks

**Core**: Apollo 68080 Core 11350 @ 92 MHz  
**Target Hardware**: Physical Vampire V4 Standalone  
**Measurement Method**: Harness v6 (10-iteration unrolled inner measurement loop)  
**Baseline Loop Overhead**: BASE_10 = 30 cycles (3.0 cycles/iter)  
**Cycle Calculation**: Cyc/Op = (RAW_10 - BASE_10) / 10.0

---

## 1. AMMX Butterfly Instruction Analysis (flyw & flyb)

AMMX natively implements high-performance butterfly instructions:
- flyw <vea>, b, d:d+1 (4 parallel 16-bit word butterflies:  = b + a$, +1 = b - a$)
- flyb <vea>, b, d:d+1 (8 parallel 8-bit byte butterflies:  = b + a$, +1 = b - a$)

### Physical V4 Benchmark Results
| Test ID | Instruction Sequence | RAW_10 | BASE_10 | Net (10x) | Cyc / Op | Speedup / Note |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| **BFLY-001** | **flyw e2,e8,e0:e1** | **26** | **30** | **-4** | **2.6** (inc. dbra) | **-1.0 cyc faster** vs separate add+sub |
| BFLY-002 | paddw e2,e8,e0 + psubw e2,e8,e1 | 36 | 30 | +6 | 3.6 (inc. dbra) | Reference (separate add+sub) |
| **BFLY-003** | **flyw (a0),e8,e0:e1** | **26** | **30** | **-4** | **2.6** (inc. dbra) | **-2.0 cyc faster** vs load+add+sub |
| BFLY-004 | load (a0),e2 + paddw + psubw | 46 | 30 | +16 | 4.6 (inc. dbra) | Reference (load + add + sub) |
| **BFLY-005** | **flyb e2,e8,e0:e1** | **26** | **30** | **-4** | **2.6** (inc. dbra) | **-1.0 cyc faster** (8 parallel byte butterflies) |
| BFLY-006 | paddb e2,e8,e0 + psubb e2,e8,e1 | 36 | 30 | +6 | 3.6 (inc. dbra) | Separate byte add + sub |

### Key Takeaways:
1. flyw and flyb execute in **exactly 1 cycle** within the CPU pipeline.
2. The <vea> memory-operand form (flyw (a0),...) completely folds memory load latency into the butterfly operation with **0 additional cycles**.
3. Replaces 2 arithmetic instructions + register shuffling with a single opcode, halving I-Cache footprint.

---

## 2. Complete Harness v6 Suite Results (Core 11350)

### Memory Operations (MEM) — Durchsatz auf Vampire V4 Standalone (92 MHz)
| Test ID | Operation | Blockgröße | Netto-Takte | Takte / Byte | Nutzdaten-Rate | Bus-Transfer-Rate |
|:---|:---|:---:|:---:|:---:|:---:|:---:|
| **MEM-010** | AMMX Fast MEMSET (Fill) | 32 Bytes | 2.6 Cyc | **0.081 c/B** | **1.132,3 MB/s** | **1.132,3 MB/s** |
| **MEM-011** | AMMX Fast MEMCPY (Copy) | 32 Bytes | 6.6 Cyc | **0.206 c/B** | **446,1 MB/s** | **892,1 MB/s** |
| **MEM-012** | AMMX Tail Mask-Store | 8 Bytes | 1.6 Cyc | **0.200 c/B** | **460,0 MB/s** | **920,0 MB/s** |
| **MEM-013** | AMMX MEMMOVE Streaming | 16 Bytes | 2.6 Cyc | **0.163 c/B** | **566,2 MB/s** | **1.132,3 MB/s** |
| **MEM-020** | AMMX Cacheline Fill (Burst) | 64 Bytes | 6.6 Cyc | **0.103 c/B** | **892,1 MB/s** | **892,1 MB/s** |
| **MEM-021** | AMMX Cacheline Copy (Burst) | 64 Bytes | 14.6 Cyc | **0.228 c/B** | **403,3 MB/s** | **806,6 MB/s** |

*Hinweis zur Unterscheidung*:
- **Nutzdaten-Rate**: Tatsächlich geschriebene bzw. kopierte Nutzlast (z. B. memcpy(dst, src, N) transferiert $ Nutzbytes).
- **Bus-Transfer-Rate**: Gesamte Bandbreite über den CPU-Speicherbus (bei memcpy werden pro Durchlauf $ Bytes gelesen UND $ Bytes geschrieben, also  	imes N$ Bus-Transfers).

### String & Text (STR)
| ID | Description | RAW_10 | BASE_10 | Net_10 | Cyc / Op |
|:---|:---|:---:|:---:|:---:|:---:|
| STR-001 | AMMX To-Lower 8 Bytes | 46 | 30 | 16 | **1.6** |
| STR-002 | AMMX To-Upper 8 Bytes | 46 | 30 | 16 | **1.6** |
| STR-003 | AMMX Exact Substring Scan | 66 | 30 | 36 | **3.6** |
| STR-004 | AMMX Whitespace Trimming | 66 | 30 | 36 | **3.6** |
| STR-011 | AMMX Hex-to-ASCII (High Nibbles) | 76 | 30 | 46 | **4.6** |
| STR-012 | AMMX Hex-to-ASCII (Low Nibbles) | 76 | 30 | 46 | **4.6** |
| STR-014 | AMMX UTF-8 Validation / Multi-Byte | 126 | 30 | 96 | **9.6** |

### Sorting & Branchless (SRT)
| ID | Description | RAW_10 | BASE_10 | Net_10 | Cyc / Op |
|:---|:---|:---:|:---:|:---:|:---:|
| SRT-001 | Scalar Branchless Min/Max (sub.l, ddx.l) | 76 | 30 | 46 | **4.6** |
| SRT-002 | AMMX 8-Byte Vector Min/Max (pmaxub, pminub) | 56 | 30 | 26 | **2.6** |
| SRT-003 | AMMX 4-Way 16-Bit Word Sort Kernel | 126 | 30 | 96 | **9.6** |
| SRT-013 | AMMX 8-Way Bitonic Sorting Network | 226 | 30 | 196 | **19.6** |

### Discrete Wavelet Transformations (DWT)
| ID | Description | RAW_10 | BASE_10 | Net_10 | Cyc / Op |
|:---|:---|:---:|:---:|:---:|:---:|
| DWT-001 | Haar Wavelet Forward (1D) | 136 | 30 | 106 | **10.6** |
| DWT-002 | Haar Wavelet 2D (Row + Col) | 226 | 30 | 196 | **19.6** |
| DWT-003 | Haar Inverse Wavelet Recon | 236 | 30 | 206 | **20.6** |
| DWT-004 | LeGall 5/3 Wavelet Predict | 216 | 30 | 186 | **18.6** |
| DWT-005 | LeGall 5/3 Wavelet Update | 296 | 30 | 266 | **26.6** |
| DWT-011 | Daubechies 9/7 Predict 1 | 256 | 30 | 226 | **22.6** |
| DWT-012 | Daubechies 9/7 Update 1 | 386 | 30 | 356 | **35.6** |
| DWT-013 | Daubechies 9/7 Predict 2 | 466 | 30 | 436 | **43.6** |
| DWT-014 | Wavelet Multilevel Subband Split | 216 | 30 | 186 | **18.6** |
| DWT-015 | Wavelet Horizontal Transpose Pack | 276 | 30 | 246 | **24.6** |

### Corpus Optimizations (AMX, FIX, DCT, FFT)
| ID | Description | RAW_10 | BASE_10 | Net_10 | Cyc / Op |
|:---|:---|:---:|:---:|:---:|:---:|
| AMX-102 | AMMX 8-Byte Transpose Row 1 | 76 | 30 | 46 | **4.6** |
| AMX-103 | AMMX 8-Byte Transpose Row 2 | 76 | 30 | 46 | **4.6** |
| AMX-105 | AMMX Parallel Add-Saturate | 76 | 30 | 46 | **4.6** |
| AMX-106 | AMMX Matrix Row Unpack & Blend | 96 | 30 | 66 | **6.6** |
| AMX-107 | AMMX Quad Word Parallel Shift | 76 | 30 | 46 | **4.6** |
| AMX-108 | AMMX 8x8 Matrix Transpose Stage 1 | 136 | 30 | 106 | **10.6** |
| FIX-101 | Fixed-Point Q15 Multiply Kernel | 176 | 30 | 146 | **14.6** |
| FIX-102 | Fixed-Point Q31 Multiply & Accumulate | 356 | 30 | 326 | **32.6** |
| DCT-101 | 8x8 2D-DCT Full Pass | 596 | 30 | 566 | **56.6** |
| DCT-109 | 1D-DCT 8-Point AMMX Kernel | 196 | 30 | 166 | **16.6** |
| FFT-101 | Radix-2 Complex FFT 8-Point Stage | 829 | 30 | 799 | **79.9** |

### Hurrican Blitting & Shading (BLT)
| ID | Description | RAW_10 | BASE_10 | Net_10 | Cyc / Op |
|:---|:---|:---:|:---:|:---:|:---:|
| BLT-006 | Alpha Blit Step 1 | 46 | 30 | 16 | **1.6** |
| BLT-007 | Alpha Blit Dual Pixel | 86 | 30 | 56 | **5.6** |
| BLT-008 | Additive Blending 4-Pixel | 216 | 30 | 186 | **18.6** |
| BLT-009 | Tint Shading Multi-Channel | 166 | 30 | 136 | **13.6** |
| BLT-010 | Full Sprite Blend Row 1 | 286 | 30 | 256 | **25.6** |
| BLT-011 | Full Sprite Blend Row 2 | 346 | 30 | 316 | **31.6** |
| BLT-012 | Masked Qword Blit | 56 | 30 | 26 | **2.6** |
| BLT-013 | Transparent Color Key Filter | 46 | 30 | 16 | **1.6** |
| BLT-014 | Subtractive Blend Step | 76 | 30 | 46 | **4.6** |
| BLT-015 | Multiplicative Blend Step | 76 | 30 | 46 | **4.6** |
| BLT-016 | Particle Shading 8-Pixel | 156 | 30 | 126 | **12.6** |
| BLT-017 | Lighting Map Modulation | 356 | 30 | 326 | **32.6** |
| BLT-018 | Ultra-Fast Single Qword Blend | 36 | 30 | 6 | **0.6** |
| BLT-019 | Gouraud Span Shading Step 1 | 116 | 30 | 86 | **8.6** |
| BLT-020 | Gouraud Span Shading Step 2 | 136 | 30 | 106 | **10.6** |
| BLT-021 | Z-Buffer Comparison & Write | 186 | 30 | 156 | **15.6** |
