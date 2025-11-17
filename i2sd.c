/*
 * i2sd.c - I2S System Daemon
 * 
 * Manages I2S device and provides additional services
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <pthread.h>

#define SOCKET_PATH "/var/run/i2sd.sock"
#define PID_FILE "/var/run/i2sd.pid"
#define I2S_DEVICE "/dev/i2s0"

/* Daemon control commands */
#define CMD_GET_STATUS 1
#define CMD_SET_VOLUME 2
#define CMD_GET_STATS 3
#define CMD_SHUTDOWN 4

typedef struct {
    int cmd;
    int param;
    char data[256];
} daemon_msg_t;

typedef struct {
    int status;
    char message[256];
} daemon_response_t;

static volatile int running = 1;
static int i2s_fd = -1;
static int socket_fd = -1;

/* Signal handler */
static void signal_handler(int sig)
{
    syslog(LOG_INFO, "Received signal %d, shutting down", sig);
    running = 0;
}

/* Daemonize process */
static int daemonize(void)
{
    pid_t pid, sid;
    
    /* Fork parent process */
    pid = fork();
    if (pid < 0) {
        return -1;
    }
    
    /* Exit parent process */
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    /* Create new session */
    sid = setsid();
    if (sid < 0) {
        return -1;
    }
    
    /* Change working directory */
    if (chdir("/") < 0) {
        return -1;
    }
    
    /* Close standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    /* Redirect to /dev/null */
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);
    
    return 0;
}

/* Write PID file */
static int write_pid_file(void)
{
    FILE *fp;
    
    fp = fopen(PID_FILE, "w");
    if (!fp) {
        syslog(LOG_ERR, "Failed to create PID file: %s", strerror(errno));
        return -1;
    }
    
    fprintf(fp, "%d\n", getpid());
    fclose(fp);
    
    return 0;
}

/* Initialize I2S device */
static int init_i2s_device(void)
{
    i2s_fd = open(I2S_DEVICE, O_RDWR);
    if (i2s_fd < 0) {
        syslog(LOG_ERR, "Failed to open I2S device: %s", strerror(errno));
        return -1;
    }
    
    syslog(LOG_INFO, "I2S device opened successfully");
    return 0;
}

/* Create Unix domain socket for IPC */
static int create_socket(void)
{
    struct sockaddr_un addr;
    
    /* Remove existing socket file */
    unlink(SOCKET_PATH);
    
    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "Failed to bind socket: %s", strerror(errno));
        close(socket_fd);
        return -1;
    }
    
    if (listen(socket_fd, 5) < 0) {
        syslog(LOG_ERR, "Failed to listen on socket: %s", strerror(errno));
        close(socket_fd);
        return -1;
    }
    
    /* Set socket permissions */
    chmod(SOCKET_PATH, 0666);
    
    syslog(LOG_INFO, "Control socket created at %s", SOCKET_PATH);
    return 0;
}

/* Handle client request */
static void handle_client(int client_fd)
{
    daemon_msg_t msg;
    daemon_response_t resp;
    ssize_t n;
    
    n = read(client_fd, &msg, sizeof(msg));
    if (n != sizeof(msg)) {
        syslog(LOG_WARNING, "Invalid message size received");
        close(client_fd);
        return;
    }
    
    memset(&resp, 0, sizeof(resp));
    
    switch (msg.cmd) {
    case CMD_GET_STATUS:
        resp.status = 0;
        snprintf(resp.message, sizeof(resp.message), 
                 "I2S daemon running, device: %s", I2S_DEVICE);
        syslog(LOG_DEBUG, "Status request received");
        break;
        
    case CMD_SET_VOLUME:
        resp.status = 0;
        snprintf(resp.message, sizeof(resp.message), 
                 "Volume set to %d", msg.param);
        syslog(LOG_INFO, "Volume set to %d", msg.param);
        break;
        
    case CMD_GET_STATS:
        resp.status = 0;
        snprintf(resp.message, sizeof(resp.message), 
                 "Uptime: %ld seconds", time(NULL));
        break;
        
    case CMD_SHUTDOWN:
        resp.status = 0;
        snprintf(resp.message, sizeof(resp.message), "Shutting down daemon");
        syslog(LOG_INFO, "Shutdown command received");
        running = 0;
        break;
        
    default:
        resp.status = -1;
        snprintf(resp.message, sizeof(resp.message), "Unknown command");
        syslog(LOG_WARNING, "Unknown command received: %d", msg.cmd);
    }
    
    write(client_fd, &resp, sizeof(resp));
    close(client_fd);
}

/* Main event loop */
static void event_loop(void)
{
    fd_set readfds;
    struct timeval tv;
    int max_fd;
    
    while (running) {
        FD_ZERO(&readfds);
        FD_SET(socket_fd, &readfds);
        max_fd = socket_fd;
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int ret = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            syslog(LOG_ERR, "select() error: %s", strerror(errno));
            break;
        }
        
        if (ret == 0)
            continue;
        
        if (FD_ISSET(socket_fd, &readfds)) {
            int client_fd = accept(socket_fd, NULL, NULL);
            if (client_fd >= 0) {
                handle_client(client_fd);
            }
        }
    }
}

/* Cleanup resources */
static void cleanup(void)
{
    if (i2s_fd >= 0)
        close(i2s_fd);
    
    if (socket_fd >= 0)
        close(socket_fd);
    
    unlink(SOCKET_PATH);
    unlink(PID_FILE);
    
    syslog(LOG_INFO, "I2S daemon terminated");
    closelog();
}

int main(int argc, char *argv[])
{
    int foreground = 0;
    
    /* Parse command line arguments */
    if (argc > 1 && strcmp(argv[1], "-f") == 0) {
        foreground = 1;
    }
    
    /* Open syslog */
    openlog("i2sd", LOG_PID | (foreground ? LOG_PERROR : 0), LOG_DAEMON);
    
    /* Daemonize unless running in foreground */
    if (!foreground) {
        if (daemonize() < 0) {
            syslog(LOG_ERR, "Failed to daemonize");
            return EXIT_FAILURE;
        }
    }
    
    syslog(LOG_INFO, "I2S daemon starting");
    
    /* Write PID file */
    if (write_pid_file() < 0) {
        return EXIT_FAILURE;
    }
    
    /* Set up signal handlers */
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, SIG_IGN);
    
    /* Initialize I2S device */
    if (init_i2s_device() < 0) {
        cleanup();
        return EXIT_FAILURE;
    }
    
    /* Create control socket */
    if (create_socket() < 0) {
        cleanup();
        return EXIT_FAILURE;
    }
    
    syslog(LOG_INFO, "I2S daemon ready");
    
    /* Main event loop */
    event_loop();
    
    /* Cleanup and exit */
    cleanup();
    return EXIT_SUCCESS;
}
