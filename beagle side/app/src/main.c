// app/src/main.c
// Unified Beagle program:
//
//  - Rotary encoder button -> send "start" to host
//  - Listen for "paper"/"plastic" -> move servo left/right, then neutral

#include "rotary.h"
#include "servo.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>

// ===================== CONFIG =====================

// *** CHANGE THIS to your VM/host IP ***
#define HOST_IP "192.168.7.1"

// Host listens for "start" on this port (host_main_server.py)
#define HOST_START_PORT   6000

// Beagle listens for classification result on this port
#define BEAGLE_CLASS_PORT 5005

#define SERVO_PERIOD_NS   20000000
#define SERVO_NEUTRAL_NS  1600000 // 1600000 for neutral
#define SERVO_MIN_NS      1200000 // 950000 is perfect clockwise for paper
#define SERVO_MAX_NS      2000000 //2300000 is perfect ccw for platic 

// servo wait time
#define SERVO_HOLD_SECONDS 5

// ==================================================

static volatile sig_atomic_t keep_running = 1;
static Servo g_servo;   // our single servo instance

static void handle_sigint(int sig)
{
    (void)sig;
    keep_running = 0;
}

// --------------------------------------------------
// Servo helper wrappers
// --------------------------------------------------
static void servo_to_neutral(void)
{
    // we just go to the neutral_ns position
    if (servo_set_pulse_ns(&g_servo, SERVO_NEUTRAL_NS) != 0) {
        perror("[main] servo_to_neutral");
    }
}

static void servo_to_paper(void)
{
    // full LEFT = min_ns
    if (servo_set_pulse_ns(&g_servo, SERVO_MIN_NS) != 0) {
        perror("[main] servo_to_paper");
    }
}

static void servo_to_plastic(void)
{
    // full RIGHT = max_ns
    if (servo_set_pulse_ns(&g_servo, SERVO_MAX_NS) != 0) {
        perror("[main] servo_to_plastic");
    }
}

// --------------------------------------------------
// SEND "start" TO HOST
// --------------------------------------------------
static int send_start_to_host(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(HOST_START_PORT);

    if (inet_pton(AF_INET, HOST_IP, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return -1;
    }

    const char *msg = "start";
    if (sendto(sock, msg, strlen(msg), 0,
               (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("sendto");
        close(sock);
        return -1;
    }

    printf("[main] Sent 'start' to %s:%d\n", HOST_IP, HOST_START_PORT);
    close(sock);
    return 0;
}

// --------------------------------------------------
// UDP SOCKET FOR RECEIVING "paper"/"plastic"
// --------------------------------------------------
static int create_result_socket(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    int enable = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(BEAGLE_CLASS_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }

    // make non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }

    printf("[main] Listening for classification on UDP %d\n", BEAGLE_CLASS_PORT);
    return sock;
}

// --------------------------------------------------

int main(void)
{
    signal(SIGINT, handle_sigint);

    // Init rotary encoder
    if (rotaryEncoder_init() != 0) {
        fprintf(stderr, "[main] ERROR: failed to init rotary encoder\n");
        return 1;
    }

    // Init servo: chip=-1 => use PWM0_CHIP env or default 0, channel=0
    if (servo_init(&g_servo,
                   -1,            // chip (override with PWM0_CHIP if needed)
                   0,             // channel
                   SERVO_PERIOD_NS,
                   SERVO_NEUTRAL_NS,
                   SERVO_MIN_NS,
                   SERVO_MAX_NS) != 0) {
        perror("[main] servo_init");
        rotaryEncoder_cleanup();
        return 1;
    }

    // Move servo to neutral at startup
    servo_to_neutral();
    printf("[main] Servo initialized to neutral.\n");

    int sock = create_result_socket();
    if (sock < 0) {
        servo_close(&g_servo);
        rotaryEncoder_cleanup();
        return 1;
    }

    bool waiting_for_result = false;
    printf("[main] Ready. Press encoder button to start.\n");

    while (keep_running) {
        // 1) Rotary button press -> send "start"
        if (!waiting_for_result && rotaryEncoder_button_pressed()) {
            printf("[main] Button press detected. Sending 'start' to host.\n");
            if (send_start_to_host() == 0) {
                waiting_for_result = true;
                printf("[main] Waiting for ML result from host...\n");
            }
            usleep(200 * 1000); // 200 ms debounce
        }

        // 2) If waiting, check for classification result
        if (waiting_for_result) {
            char buf[64];
            struct sockaddr_in src;
            socklen_t slen = sizeof(src);

            ssize_t n = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                                 (struct sockaddr *)&src, &slen);
            if (n > 0) {
                buf[n] = '\0';
                printf("[main] Received: '%s'\n", buf);

                if (strcmp(buf, "paper") == 0) {
                    printf("[main] PAPER → move servo LEFT\n");
                    servo_to_paper();
                    sleep(SERVO_HOLD_SECONDS);
                    servo_to_neutral();
                    printf("[main] Servo back to neutral.\n");
                    waiting_for_result = false;
                }
                else if (strcmp(buf, "plastic") == 0) {
                    printf("[main] PLASTIC → move servo RIGHT\n");
                    servo_to_plastic();
                    sleep(SERVO_HOLD_SECONDS);
                    servo_to_neutral();
                    printf("[main] Servo back to neutral.\n");
                    waiting_for_result = false;
                }
                else {
                    printf("[main] Unknown classification message, ignoring.\n");
                }
            }
        }

        usleep(5 * 1000); // 5 ms loop
    }

    close(sock);
    servo_close(&g_servo);
    rotaryEncoder_cleanup();
    printf("[main] Exiting.\n");
    return 0;
}
