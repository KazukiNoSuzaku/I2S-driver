/*
 * libi2s.h - I2S User Space Library Header
 */

#ifndef LIBI2S_H
#define LIBI2S_H

#include <stddef.h>
#include <stdint.h>

/* I2S handle */
typedef struct i2s_handle_s *i2s_handle_t;

/* I2S configuration */
typedef struct {
    int sample_rate;
    int bit_depth;
    int channels;
} i2s_config_t;

/* I2S status */
typedef enum {
    I2S_STATUS_STOPPED = 0,
    I2S_STATUS_RUNNING = 1,
    I2S_STATUS_ERROR = -1
} i2s_status_t;

/* Library functions */
i2s_handle_t i2s_open(const char *device);
void i2s_close(i2s_handle_t handle);

int i2s_configure(i2s_handle_t handle, const i2s_config_t *config);
int i2s_get_config(i2s_handle_t handle, i2s_config_t *config);

int i2s_start(i2s_handle_t handle);
int i2s_stop(i2s_handle_t handle);

i2s_status_t i2s_get_status(i2s_handle_t handle);

ssize_t i2s_read(i2s_handle_t handle, void *buffer, size_t size);
ssize_t i2s_write(i2s_handle_t handle, const void *buffer, size_t size);

const char *i2s_get_error(i2s_handle_t handle);

/* Daemon communication functions */
int i2s_daemon_connect(void);
void i2s_daemon_disconnect(int sock);
int i2s_daemon_send_command(int sock, int cmd, int param);

#endif /* LIBI2S_H */

/*
 * libi2s.c - I2S User Space Library Implementation
 */

#include "libi2s.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>

/* IOCTL commands (must match kernel driver) */
#define I2S_IOC_MAGIC 'i'
#define I2S_SET_SAMPLE_RATE _IOW(I2S_IOC_MAGIC, 1, int)
#define I2S_GET_SAMPLE_RATE _IOR(I2S_IOC_MAGIC, 2, int)
#define I2S_SET_BIT_DEPTH _IOW(I2S_IOC_MAGIC, 3, int)
#define I2S_GET_BIT_DEPTH _IOR(I2S_IOC_MAGIC, 4, int)
#define I2S_START _IO(I2S_IOC_MAGIC, 5)
#define I2S_STOP _IO(I2S_IOC_MAGIC, 6)
#define I2S_GET_STATUS _IOR(I2S_IOC_MAGIC, 7, int)

#define DAEMON_SOCKET_PATH "/var/run/i2sd.sock"

/* Internal handle structure */
struct i2s_handle_s {
    int fd;
    char error_msg[256];
    i2s_config_t config;
};

/* Daemon message structures */
typedef struct {
    int cmd;
    int param;
    char data[256];
} daemon_msg_t;

typedef struct {
    int status;
    char message[256];
} daemon_response_t;

/* Open I2S device */
i2s_handle_t i2s_open(const char *device)
{
    i2s_handle_t handle;
    
    if (!device) {
        device = "/dev/i2s0";
    }
    
    handle = malloc(sizeof(struct i2s_handle_s));
    if (!handle) {
        return NULL;
    }
    
    memset(handle, 0, sizeof(struct i2s_handle_s));
    
    handle->fd = open(device, O_RDWR);
    if (handle->fd < 0) {
        snprintf(handle->error_msg, sizeof(handle->error_msg),
                 "Failed to open device %s: %s", device, strerror(errno));
        free(handle);
        return NULL;
    }
    
    /* Get current configuration */
    i2s_get_config(handle, &handle->config);
    
    return handle;
}

/* Close I2S device */
void i2s_close(i2s_handle_t handle)
{
    if (!handle)
        return;
    
    if (handle->fd >= 0) {
        i2s_stop(handle);
        close(handle->fd);
    }
    
    free(handle);
}

/* Configure I2S device */
int i2s_configure(i2s_handle_t handle, const i2s_config_t *config)
{
    if (!handle || !config) {
        return -1;
    }
    
    /* Set sample rate */
    if (ioctl(handle->fd, I2S_SET_SAMPLE_RATE, &config->sample_rate) < 0) {
        snprintf(handle->error_msg, sizeof(handle->error_msg),
                 "Failed to set sample rate: %s", strerror(errno));
        return -1;
    }
    
    /* Set bit depth */
    if (ioctl(handle->fd, I2S_SET_BIT_DEPTH, &config->bit_depth) < 0) {
        snprintf(handle->error_msg, sizeof(handle->error_msg),
                 "Failed to set bit depth: %s", strerror(errno));
        return -1;
    }
    
    handle->config = *config;
    return 0;
}

/* Get current configuration */
int i2s_get_config(i2s_handle_t handle, i2s_config_t *config)
{
    if (!handle || !config) {
        return -1;
    }
    
    if (ioctl(handle->fd, I2S_GET_SAMPLE_RATE, &config->sample_rate) < 0) {
        snprintf(handle->error_msg, sizeof(handle->error_msg),
                 "Failed to get sample rate: %s", strerror(errno));
        return -1;
    }
    
    if (ioctl(handle->fd, I2S_GET_BIT_DEPTH, &config->bit_depth) < 0) {
        snprintf(handle->error_msg, sizeof(handle->error_msg),
                 "Failed to get bit depth: %s", strerror(errno));
        return -1;
    }
    
    config->channels = 2; /* Default stereo */
    return 0;
}

/* Start I2S transmission */
int i2s_start(i2s_handle_t handle)
{
    if (!handle) {
        return -1;
    }
    
    if (ioctl(handle->fd, I2S_START) < 0) {
        snprintf(handle->error_msg, sizeof(handle->error_msg),
                 "Failed to start I2S: %s", strerror(errno));
        return -1;
    }
    
    return 0;
}

/* Stop I2S transmission */
int i2s_stop(i2s_handle_t handle)
{
    if (!handle) {
        return -1;
    }
    
    if (ioctl(handle->fd, I2S_STOP) < 0) {
        snprintf(handle->error_msg, sizeof(handle->error_msg),
                 "Failed to stop I2S: %s", strerror(errno));
        return -1;
    }
    
    return 0;
}

/* Get I2S status */
i2s_status_t i2s_get_status(i2s_handle_t handle)
{
    int status;
    
    if (!handle) {
        return I2S_STATUS_ERROR;
    }
    
    if (ioctl(handle->fd, I2S_GET_STATUS, &status) < 0) {
        snprintf(handle->error_msg, sizeof(handle->error_msg),
                 "Failed to get status: %s", strerror(errno));
        return I2S_STATUS_ERROR;
    }
    
    return status ? I2S_STATUS_RUNNING : I2S_STATUS_STOPPED;
}

/* Read audio data from I2S */
ssize_t i2s_read(i2s_handle_t handle, void *buffer, size_t size)
{
    ssize_t ret;
    
    if (!handle || !buffer) {
        return -1;
    }
    
    ret = read(handle->fd, buffer, size);
    if (ret < 0) {
        snprintf(handle->error_msg, sizeof(handle->error_msg),
                 "Read failed: %s", strerror(errno));
    }
    
    return ret;
}

/* Write audio data to I2S */
ssize_t i2s_write(i2s_handle_t handle, const void *buffer, size_t size)
{
    ssize_t ret;
    
    if (!handle || !buffer) {
        return -1;
    }
    
    ret = write(handle->fd, buffer, size);
    if (ret < 0) {
        snprintf(handle->error_msg, sizeof(handle->error_msg),
                 "Write failed: %s", strerror(errno));
    }
    
    return ret;
}

/* Get last error message */
const char *i2s_get_error(i2s_handle_t handle)
{
    if (!handle) {
        return "Invalid handle";
    }
    
    return handle->error_msg;
}

/* Connect to daemon */
int i2s_daemon_connect(void)
{
    int sock;
    struct sockaddr_un addr;
    
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, DAEMON_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    return sock;
}

/* Disconnect from daemon */
void i2s_daemon_disconnect(int sock)
{
    if (sock >= 0) {
        close(sock);
    }
}

/* Send command to daemon */
int i2s_daemon_send_command(int sock, int cmd, int param)
{
    daemon_msg_t msg;
    daemon_response_t resp;
    
    if (sock < 0) {
        return -1;
    }
    
    memset(&msg, 0, sizeof(msg));
    msg.cmd = cmd;
    msg.param = param;
    
    if (write(sock, &msg, sizeof(msg)) != sizeof(msg)) {
        return -1;
    }
    
    if (read(sock, &resp, sizeof(resp)) != sizeof(resp)) {
        return -1;
    }
    
    return resp.status;
}
