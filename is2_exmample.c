/*
 * i2s_example.c - Example application using libi2s
 * 
 * Demonstrates how to use the I2S library
 */

#include "libi2s.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#define SAMPLE_RATE 44100
#define BIT_DEPTH 16
#define DURATION 2  /* seconds */
#define FREQUENCY 440.0  /* A4 note */

/* Generate sine wave */
void generate_sine_wave(int16_t *buffer, size_t samples, double frequency, int sample_rate)
{
    for (size_t i = 0; i < samples; i++) {
        double t = (double)i / sample_rate;
        double value = sin(2.0 * M_PI * frequency * t);
        buffer[i] = (int16_t)(value * 32767.0);
    }
}

int main(int argc, char *argv[])
{
    i2s_handle_t handle;
    i2s_config_t config;
    i2s_status_t status;
    int16_t *audio_buffer;
    size_t buffer_size;
    ssize_t written;
    
    printf("I2S Example Application\n");
    printf("========================\n\n");
    
    /* Open I2S device */
    printf("Opening I2S device...\n");
    handle = i2s_open(NULL);  /* Use default device */
    if (!handle) {
        fprintf(stderr, "Error: Failed to open I2S device\n");
        return EXIT_FAILURE;
    }
    printf("I2S device opened successfully\n\n");
    
    /* Configure I2S */
    printf("Configuring I2S:\n");
    printf("  Sample Rate: %d Hz\n", SAMPLE_RATE);
    printf("  Bit Depth: %d bits\n", BIT_DEPTH);
    
    config.sample_rate = SAMPLE_RATE;
    config.bit_depth = BIT_DEPTH;
    config.channels = 2;
    
    if (i2s_configure(handle, &config) < 0) {
        fprintf(stderr, "Error: %s\n", i2s_get_error(handle));
        i2s_close(handle);
        return EXIT_FAILURE;
    }
    printf("I2S configured successfully\n\n");
    
    /* Get and display current configuration */
    if (i2s_get_config(handle, &config) == 0) {
        printf("Current Configuration:\n");
        printf("  Sample Rate: %d Hz\n", config.sample_rate);
        printf("  Bit Depth: %d bits\n", config.bit_depth);
        printf("  Channels: %d\n\n", config.channels);
    }
    
    /* Start I2S transmission */
    printf("Starting I2S transmission...\n");
    if (i2s_start(handle) < 0) {
        fprintf(stderr, "Error: %s\n", i2s_get_error(handle));
        i2s_close(handle);
        return EXIT_FAILURE;
    }
    printf("I2S transmission started\n\n");
    
    /* Check status */
    status = i2s_get_status(handle);
    printf("I2S Status: %s\n\n", 
           status == I2S_STATUS_RUNNING ? "RUNNING" : "STOPPED");
    
    /* Generate audio data */
    buffer_size = SAMPLE_RATE * DURATION;
    audio_buffer = malloc(buffer_size * sizeof(int16_t));
    if (!audio_buffer) {
        fprintf(stderr, "Error: Failed to allocate audio buffer\n");
        i2s_stop(handle);
        i2s_close(handle);
        return EXIT_FAILURE;
    }
    
    printf("Generating %d Hz sine wave for %d seconds...\n", 
           (int)FREQUENCY, DURATION);
    generate_sine_wave(audio_buffer, buffer_size, FREQUENCY, SAMPLE_RATE);
    
    /* Write audio data */
    printf("Writing audio data to I2S...\n");
    written = i2s_write(handle, audio_buffer, buffer_size * sizeof(int16_t));
    if (written < 0) {
        fprintf(stderr, "Error: %s\n", i2s_get_error(handle));
        free(audio_buffer);
        i2s_stop(handle);
        i2s_close(handle);
        return EXIT_FAILURE;
    }
    printf("Wrote %zd bytes to I2S\n\n", written);
    
    /* Demonstrate reading (would read from I2S input) */
    printf("Reading audio data from I2S...\n");
    int16_t read_buffer[1024];
    ssize_t read_bytes = i2s_read(handle, read_buffer, sizeof(read_buffer));
    if (read_bytes > 0) {
        printf("Read %zd bytes from I2S\n\n", read_bytes);
    }
    
    /* Test daemon communication */
    printf("Testing daemon communication...\n");
    int daemon_sock = i2s_daemon_connect();
    if (daemon_sock >= 0) {
        printf("Connected to I2S daemon\n");
        
        /* Send status query command */
        if (i2s_daemon_send_command(daemon_sock, 1, 0) == 0) {
            printf("Daemon status query successful\n");
        }
        
        i2s_daemon_disconnect(daemon_sock);
        printf("Disconnected from daemon\n\n");
    } else {
        printf("Could not connect to daemon (it may not be running)\n\n");
    }
    
    /* Cleanup */
    printf("Stopping I2S transmission...\n");
    i2s_stop(handle);
    
    free(audio_buffer);
    i2s_close(handle);
    
    printf("I2S device closed\n");
    printf("\nExample completed successfully!\n");
    
    return EXIT_SUCCESS;
}
