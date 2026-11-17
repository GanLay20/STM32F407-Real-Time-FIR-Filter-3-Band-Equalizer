# STM32F407 Real-Time FIR Filter and 3-Band Equalizer

This project runs a 3-band audio equalizer in real time on an STM32F407. An analog signal comes in through the ADC at 48 kHz. Each block of samples is filtered with FIR filters from the CMSIS-DSP library, and the result goes back out through the DAC.

I built it in three steps. First I got the ADC to DAC path working with no processing at all. Then I added a single FIR filter. Finally I split the signal into three bands with separate gains to make an equalizer. I also wrote a couple of small programs to understand FIR filtering and convolution without relying on the library.

## Table of Contents

1. [How the Signal Flows](#how-the-signal-flows)
2. [Hardware Setup](#hardware-setup)
3. [Project Files](#project-files)
4. [How It Works](#how-it-works)
5. [Filter Design](#filter-design)
6. [Timing and Performance](#timing-and-performance)
7. [Building and Running](#building-and-running)
8. [Testing](#testing)
9. [Known Limitations](#known-limitations)
10. [License](#license)

## How the Signal Flows

```
             TIM2 at 48 kHz (triggers both the ADC and the DAC)
                 |                                      |
                 v                                      v
Analog in -> ADC1 (PA0) -> DMA -> ADC_Buffer      DAC_Buffer -> DMA -> DAC (PA4) -> Analog out
                                     |                  ^
                          half/full  |                  |
                          callback   v                  |
                        process_task(): convert, filter, write back
```

The equalizer splits the signal into three bands, scales each one and adds them back together:

```
          +-> Low-pass   0 to 1 kHz    -> x 0.3 -+
input ----+-> Band-pass  1 to 4 kHz    -> x 0.2 -+-> sum -> output
          +-> High-pass  4 to 24 kHz   -> x 0.1 -+
```

## Hardware Setup

I used an STM32F4-Discovery board. All peripheral settings come from `PingPong.ioc`, so you can open that file in STM32CubeMX and regenerate the project.

| Item | Setting |
|---|---|
| MCU | STM32F407VGT6 (Cortex-M4 with FPU) |
| System clock | 168 MHz from the 8 MHz external crystal |
| Sample timer | TIM2, 84 MHz / 1750 = 48 kHz |
| Input | ADC1 channel 0 on PA0, 12-bit |
| Output | DAC channel 1 on PA4, 12-bit |
| DMA | Circular mode for both the ADC and the DAC |

The ADC and DAC are both triggered by TIM2. Because they share the same clock, input and output always stay in step and never drift apart.

The ADC reads 0 V to 3.3 V, so the input signal has to sit on a DC bias of about 1.6 V. Without it, the negative half of the waveform gets cut off. Never put more than 3.3 V on PA0.

## Project Files

```
PingPong.ioc                  CubeMX configuration
Core/Src/main.c               Peripheral setup, then calls AppMain()
src/
  AppMain.c                   Step 1: ADC to DAC pass-through
  AppMainFIR.c                Step 2: one FIR filter
  AppMainEqualizer.c          Step 3: 3-band equalizer
  FIR_direct_lpf_coefficients.h
  FIR_direct_bpf_coefficients.h
  FIR_direct_hpf_coefficients.h
experiments/
  FIR1.c                      FIR filter written in plain C
  convolution.c               Direct convolution on a test signal
  FIR_lpf_coefficients.h      64-tap low-pass used by convolution.c
```

Each `AppMain*.c` file is a complete program on its own. Only add one of them to the build at a time, because they all define `AppMain()` and the DMA callbacks.

## How It Works

### Starting up

`AppMain()` starts the ADC and DAC with DMA first, then starts TIM2. Doing it in this order means the first sample and the first output happen on the same timer tick.

```c
HAL_ADC_Start_DMA(&hadc1, ADC_Buffer, FRAME_LENGTH);
HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, DAC_Buffer, FRAME_LENGTH, DAC_ALIGN_12B_R);
HAL_TIM_Base_Start(&htim2);
DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM2_STOP;
```

The last line freezes TIM2 when the debugger stops the CPU. That keeps the buffers stable while you look at them in the watch window.

### Ping-pong DMA

`ADC_Buffer` and `DAC_Buffer` each hold 256 samples, split into two halves of 128. The DMA runs in circular mode and calls back into the code twice per cycle:

| Callback | ADC just filled | DAC is reading | Code works on |
|---|---|---|---|
| Half complete | first half | second half | first half |
| Full complete | second half | first half | second half |

When a callback fires, the DAC is busy with the other half of its buffer. So the code can safely write new output into the half that's free. The callbacks only set two pointers and a flag. All the real work happens in the main loop, which keeps the interrupts short.

### Processing a frame

The main loop calls `process_task()` whenever the flag is set. It does four things:

1. Converts the new ADC samples to float and subtracts the DC offset, so the signal is centred on zero.
2. Adds the offset back to the previous frame's result and writes it into the DAC buffer.
3. Rotates the three working buffers.
4. Filters the frame that just came in.

The cast to float happens before the offset is subtracted. If you subtract first, values below the offset wrap around as unsigned integers instead of becoming negative.

### Triple buffering

There are three working buffers. Each frame, the code swaps their pointers instead of copying any data:

| Buffer | Becomes | What happens to it next |
|---|---|---|
| `input` (just filled) | `process` | Gets filtered right away |
| `process` (just filtered) | `output` | Goes to the DAC on the next frame |
| `output` (just sent) | `input` | Gets refilled on the next frame |

This separates collecting, filtering and sending, so no buffer is ever read and written at the same time.

### Step 1: Pass-through

`AppMain.c` does no filtering. It copies each sample through and adds the offset back. This file also calculates the DC offset fresh for every frame by averaging the samples. Its only purpose is to prove that the whole path from ADC to DAC works before adding any DSP.

### Step 2: Single FIR filter

`AppMainFIR.c` sets up a CMSIS FIR filter once at startup and runs it on every frame:

```c
arm_fir_init_f32(&S_hpf, K, h_high, firStateF32_hpf, n_sample);
arm_fir_f32(&S_hpf, process1, process1, n_sample);
```

The filter keeps its last 128 input samples in a state buffer between calls. That's why the output stays smooth across frame boundaries, with no clicks where one frame ends and the next begins. To try the low-pass or band-pass instead, uncomment the matching line.

### Step 3: Equalizer

`AppMainEqualizer.c` runs all three filters on the same frame, scales each band, then adds them back together:

```c
arm_fir_f32(&S_lpf, process1, lpf_out, n_sample);
arm_fir_f32(&S_bpf, process1, bpf_out, n_sample);
arm_fir_f32(&S_hpf, process1, hpf_out, n_sample);

arm_scale_f32(lpf_out, amp_low,  lpf_out, n_sample);
arm_scale_f32(bpf_out, amp_band, bpf_out, n_sample);
arm_scale_f32(hpf_out, amp_high, hpf_out, n_sample);

arm_add_f32(lpf_out, bpf_out, process1, n_sample);
arm_add_f32(hpf_out, process1, process1, n_sample);
```

You change the gains with `amp_low`, `amp_band` and `amp_high` at the top of the file.

## Filter Design

All three filters have 129 taps and symmetric coefficients, which gives them linear phase. They were designed for a 48 kHz sample rate.

| Filter | Taps | Band (at -6 dB) | Delay |
|---|---|---|---|
| Low-pass | 129 | 0 to 1 kHz | 64 samples (1.33 ms) |
| Band-pass | 129 | 1 to 4 kHz | 64 samples (1.33 ms) |
| High-pass | 129 | 4 to 24 kHz | 64 samples (1.33 ms) |

Two things make these work well together as an equalizer:

- The bands meet at 1 kHz and 4 kHz, each crossing at -6 dB, so together they cover the whole spectrum with no gaps or overlaps.
- All three filters delay the signal by exactly the same 64 samples, so the bands stay lined up in time when they're added back together.

Because of this, setting all three gains to 1.0 gives you back the original signal, just delayed. The band edges depend on the sample rate. If you run the same coefficients at 16 kHz, the crossovers move to about 333 Hz and 1.33 kHz.

## Timing and Performance

These numbers are for the equalizer build.

| Item | Value |
|---|---|
| Frame size | 128 samples |
| Time available per frame | 2.67 ms |
| Work per frame | about 49,500 multiply-accumulates (3 filters x 129 taps x 128 samples) |
| Total delay, input to output | about 9.3 ms |
| Flash used | about 14 KB |
| RAM used | about 13 KB |

The delay comes from two places. The triple buffer holds each sample for three frames, which is about 8 ms. The FIR filters add another 1.33 ms. I worked this out from the code rather than measuring it on a scope.

## Building and Running

You'll need:

- An STM32F4-Discovery board, or any STM32F407VG board
- Keil MDK-ARM with the STM32F4 device pack
- The CMSIS-DSP pack
- STM32CubeMX
- A signal generator and an oscilloscope

Steps:

1. Open `PingPong.ioc` in STM32CubeMX and generate code for MDK-ARM.
2. In Keil, open Manage Run-Time Environment and turn on CMSIS DSP.
3. Add one of the `AppMain*.c` files from `src/` to the project, along with the coefficient headers it uses.
4. Under Options for Target, set Floating Point Hardware to Single Precision.
5. Check that `main.c` calls `AppMain()` after all the `MX_..._Init()` functions.
6. Build and flash.

Connect the biased input signal to PA0, the scope to PA4, and share ground between them.

## Testing

Here's how I check each step on the scope:

- **Pass-through:** put a 1 kHz sine wave into PA0. The same wave should come out of PA4, delayed by a few milliseconds.
- **Single FIR, high-pass:** sweep the input from 100 Hz to 10 kHz. The output should be very small below about 4 kHz and pass through above it.
- **Equalizer:** set all three gains to 1.0 and the output should look the same as the input. Set one gain to 0.0 and that band should disappear from the output.

If something looks wrong, pause in the debugger and watch `ADC_Buffer`, `DAC_Buffer`, `input1`, `process1` and `output1`. Since TIM2 freezes at breakpoints, what you see in the watch window is a consistent snapshot.

## Experiments

These files aren't part of the real-time system. I wrote them to understand what the library does underneath.

- **`FIR1.c`** is an FIR filter in plain C. It keeps a delay line of past samples, shifts it forward on each call, appends the new block, and multiplies each output against the coefficients by hand. The demo uses a 5-tap moving average.
- **`convolution.c`** works out y[n] = sum of h[k] x[n-k] directly. The input is two sine waves mixed together, one slow and one fast. After the 64-tap low-pass, only the slow one is left. The result is sent to Keil's logic analyzer so you can see the waveform.

## Known Limitations

There are a few things I'd still like to improve:

- The flag and pointers shared between the DMA callbacks and the main loop should be declared `volatile`.
- The DAC value isn't limited to 0 to 4095, so large signals with high gains can wrap around.
- There's a `_printf` call inside the processing loop that I used for debugging. It should come out of the real-time path.
- If processing ever takes longer than one frame, the code doesn't detect it and just drops the frame.
- CubeMX sets up ADC1 and ADC2 in dual mode, but only ADC1 is started. The second channel isn't used yet.
- The DC offset is fixed at 2000 in the filter builds. A different bias voltage would need a different value.

## License

The application code is released under the MIT License. See `LICENSE` for details. The STM32 HAL and CMSIS files belong to STMicroelectronics and Arm and follow their own licenses.
