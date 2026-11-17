# STM32F407-Real-Time-FIR-Filter-3-Band-Equalizer
A 3-band FIR audio equalizer running in real time on an STM32F407. It samples at 48 kHz with the ADC, filters the signal using CMSIS-DSP, and plays it back through the DAC, using ping-pong DMA and triple buffering.
