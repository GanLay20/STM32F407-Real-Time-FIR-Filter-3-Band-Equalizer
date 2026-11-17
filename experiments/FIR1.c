#include <stdio.h>
#include <string.h> // For memset and memmove

// Define filter parameters
#define FILTER_LENGTH 5 // Number of filter coefficients
#define MAX_INPUT_LEN 10 // Max length of input signal to process at once

// Global buffer to store input samples (acting as a delay line)
double input_buffer[FILTER_LENGTH + MAX_INPUT_LEN - 1]; 

// Function to initialize the filter (clear the buffer)
void fir_init(void) {
    memset(input_buffer, 0, sizeof(input_buffer));
}

// FIR filter function
// coeffs: array of filter coefficients
// input: array of new input samples
// output: array to store filtered output samples
// length: number of new input samples to process
// filterLength: number of filter coefficients
void fir_filter(const double *coeffs, const double *input, double *output, int length, int filterLength) {
    double accumulator; // Accumulator for MAC operations
    const double *coeff_ptr; // Pointer to coefficients
    const double *input_ptr; // Pointer to input samples

    // Copy new input samples to the end of the buffer
    // This effectively shifts older samples to the left
    memmove(&input_buffer[0], &input_buffer[length], (filterLength - 1) * sizeof(double));
    memcpy(&input_buffer[filterLength - 1], input, length * sizeof(double));

    // Apply the filter to each new input sample
    for (int n = 0; n < length; n++) {
        accumulator = 0;
        coeff_ptr = coeffs;
        input_ptr = &input_buffer[filterLength - 1 + n]; // Point to the current input sample in the buffer

        // Perform the convolution (MAC operation)
        for (int k = 0; k < filterLength; k++) {
            accumulator += (*coeff_ptr++) * (*input_ptr--); // Multiply and accumulate
        }
        output[n] = accumulator;
    }
}

int FIRmain() {
    // Example filter coefficients (e.g., a simple moving average filter)
    double fir_coeffs[FILTER_LENGTH] = {0.2, 0.2, 0.2, 0.2, 0.2}; 

    // Example input signal
    double input_signal[MAX_INPUT_LEN] = {1.0, 0.5, 0.2, 0.8, 0.1, 0.9, 0.3, 0.7, 0.4, 0.6};
    double output_signal[MAX_INPUT_LEN];

    fir_init(); // Initialize the filter buffer

    // Process the input signal
    fir_filter(fir_coeffs, input_signal, output_signal, MAX_INPUT_LEN, FILTER_LENGTH);

    // Print the output signal
    printf("Filtered Output:\n");
    for (int i = 0; i < MAX_INPUT_LEN; i++) {
        printf("%.4f ", output_signal[i]);
    }
    printf("\n");

    return 0;
}