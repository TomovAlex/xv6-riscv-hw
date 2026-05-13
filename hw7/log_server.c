#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FIFO_PATH "/tmp/log_server.fifo"
#define LOG_PATH "/tmp/log_server.log"
#define ALARM_SEC 10
#define BUFFER_SIZE 4096

static volatile sig_atomic_t shutdown_signal = 0;
static volatile sig_atomic_t alarm_requested = 0;
static volatile sig_atomic_t stats_requested = 0;
static volatile sig_atomic_t daemonize_requested = 0;

struct server_stats {
    unsigned long messages;
    unsigned long long bytes;
    unsigned long alarms;
};

static struct server_stats stats = {0, 0, 0};

static void signal_handler(int signum) {
    if (signum == SIGTERM || signum == SIGINT)
        shutdown_signal = signum;
    else if (signum == SIGALRM)
        alarm_requested = 1;
    else if (signum == SIGUSR1)
        stats_requested = 1;
    else if (signum == SIGHUP)
        daemonize_requested = 1;
}

static int install_signal_handler(int signum, void (*handler)(int)) {
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handler;

    if (sigemptyset(&action.sa_mask) != 0) {
        fprintf(stderr, "Cannot initialize signal mask: %s\n", strerror(errno));
        return -1;
    }

    if (sigaction(signum, &action, NULL) != 0) {
        fprintf(stderr, "Cannot install handler for signal %d: %s\n", signum, strerror(errno));
        return -1;
    }

    return 0;
}

static int setup_signals() {
    if (install_signal_handler(SIGTERM, signal_handler) != 0)
        return -1;

    if (install_signal_handler(SIGINT, signal_handler) != 0)
        return -1;

    if (install_signal_handler(SIGALRM, signal_handler) != 0)
        return -1;

    if (install_signal_handler(SIGUSR1, signal_handler) != 0)
        return -1;

    if (install_signal_handler(SIGHUP, signal_handler) != 0)
        return -1;

    if (install_signal_handler(SIGQUIT, SIG_IGN) != 0)
        return -1;

    return 0;
}

static void print_stats() {
    printf("Statistics:\n");
    printf("  messages: %lu\n", stats.messages);
    printf("  bytes: %llu\n", stats.bytes);
    printf("  alarms: %lu\n", stats.alarms);
}

static int setup_logging(int daemon_mode) {
    if (!daemon_mode)
        return 0;

    if (freopen(LOG_PATH, "a", stdout) == NULL) {
        fprintf(stderr, "Cannot open log file %s: %s\n", LOG_PATH, strerror(errno));
        return -1;
    }

    if (freopen(LOG_PATH, "a", stderr) == NULL) {
        fprintf(stdout, "Cannot redirect stderr to log file %s: %s\n",
                LOG_PATH, strerror(errno));
        return -1;
    }

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    return 0;
}

static int become_daemon() {
    fflush(NULL);

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Cannot fork daemon process: %s\n", strerror(errno));
        return -1;
    }

    if (pid > 0)
        _exit(0);

    if (setsid() == -1) {
        fprintf(stderr, "Cannot create new session: %s\n", strerror(errno));
        return -1;
    }

    pid_t pid2 = fork();
    if (pid2 < 0) {
        fprintf(stderr, "Cannot fork daemon process: %s\n", strerror(errno));
        return -1;
    }

    if (pid2 > 0)
        _exit(0);

    umask(0);

    if (chdir("/") != 0) {
        fprintf(stderr, "Cannot change directory to /: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static int null_stdin() {
    int null_fd = open("/dev/null", O_RDONLY);
    if (null_fd == -1) {
        fprintf(stderr, "Cannot open /dev/null: %s\n", strerror(errno));
        return -1;
    }

    if (dup2(null_fd, STDIN_FILENO) == -1) {
        fprintf(stderr, "Cannot redirect stdin: %s\n", strerror(errno));
        close(null_fd);
        return -1;
    }

    if (null_fd != STDIN_FILENO && close(null_fd) != 0) {
        fprintf(stderr, "Cannot close /dev/null: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static int start_daemon_mode(int by_signal) {
    if (become_daemon() != 0)
        return -1;

    if (setup_logging(1) != 0)
        return -1;

    if (null_stdin() != 0)
        return -1;

    if (by_signal) {
        printf("Daemonized after SIGHUP\n");
        print_stats();
    } else
        printf("Started as daemon\n");

    return 0;
}

static int handle_requests(int *daemon_mode, int fifo_fd, int *finish_after_eof) {
    if (daemonize_requested) {
        daemonize_requested = 0;

        if (*daemon_mode)
            printf("Received SIGHUP, already running as daemon\n");
        else {
            if (start_daemon_mode(1) != 0)
                return -1;
            *daemon_mode = 1;
            alarm(ALARM_SEC);
        }
    }

    if (alarm_requested) {
        alarm_requested = 0;
        stats.alarms++;
        printf("Diagnostic: log server is running and waiting for data\n");
        alarm(ALARM_SEC);
    }

    if (stats_requested) {
        stats_requested = 0;
        printf("Received SIGUSR1, printing statistics\n");
        print_stats();
    }

    if (shutdown_signal == SIGTERM) {
        printf("Received SIGTERM, stopping without reading remaining FIFO data\n");
        return 1;
    }

    if (shutdown_signal == SIGINT) {
        if (fifo_fd >= 0 && finish_after_eof != NULL) {
            if (!*finish_after_eof) {
                printf("Received SIGINT, reading current FIFO until EOF\n");
                *finish_after_eof = 1;
            }
            return 0;
        }

        printf("Received SIGINT while waiting for FIFO\n");
        return 1;
    }

    return 0;
}

static int prepare_fifo(int *created_fifo) {
    struct stat st;

    *created_fifo = 0;

    if (mkfifo(FIFO_PATH, 0600) == 0) {
        *created_fifo = 1;
        printf("Created FIFO: %s\n", FIFO_PATH);
        return 0;
    }

    if (errno != EEXIST) {
        fprintf(stderr, "Cannot create FIFO %s: %s\n", FIFO_PATH, strerror(errno));
        return -1;
    }

    if (stat(FIFO_PATH, &st) != 0) {
        fprintf(stderr, "Cannot stat %s: %s\n", FIFO_PATH, strerror(errno));
        return -1;
    }

    if (!S_ISFIFO(st.st_mode)) {
        fprintf(stderr, "%s already exists, but it is not a FIFO\n", FIFO_PATH);
        return -1;
    }

    printf("Using existing FIFO: %s\n", FIFO_PATH);
    return 0;
}

static int run_server(int *daemon_mode) {
    char buffer[BUFFER_SIZE + 1];

    while (shutdown_signal == 0) {
        int fifo_fd;
        int finish_after_eof = 0;
        int has_data = 0;
        char last_char = '\0';
        int request_result;

        request_result = handle_requests(daemon_mode, -1, NULL);
        if (request_result < 0)
            return -1;
        if (request_result > 0)
            break;

        fifo_fd = open(FIFO_PATH, O_RDONLY);
        if (fifo_fd == -1) {
            if (errno == EINTR) {
                request_result = handle_requests(daemon_mode, -1, NULL);
                if (request_result < 0)
                    return -1;
                if (request_result > 0)
                    break;
                continue;
            }
            fprintf(stderr, "Cannot open FIFO %s: %s\n", FIFO_PATH, strerror(errno));
            return -1;
        }

        printf("FIFO opened for reading\n");

        while (1) {
            ssize_t bytes_read;

            request_result = handle_requests(daemon_mode, fifo_fd, &finish_after_eof);
            if (request_result < 0) {
                close(fifo_fd);
                return -1;
            }
            if (request_result > 0) {
                close(fifo_fd);
                return 0;
            }

            bytes_read = read(fifo_fd, buffer, BUFFER_SIZE);
            if (bytes_read > 0) {
                has_data = 1;
                last_char = buffer[bytes_read - 1];
                stats.bytes += (unsigned long long)bytes_read;
                buffer[bytes_read] = '\0';
                fputs(buffer, stdout);
                fflush(stdout);

                request_result = handle_requests(daemon_mode, fifo_fd, &finish_after_eof);
                if (request_result < 0) {
                    close(fifo_fd);
                    return -1;
                }
                if (request_result > 0) {
                    close(fifo_fd);
                    return 0;
                }

                continue;
            }

            if (bytes_read == 0) {
                if (has_data && last_char != '\n')
                    putchar('\n');

                printf("Writer closed FIFO\n");
                break;
            }

            if (errno == EINTR) {
                request_result = handle_requests(daemon_mode, fifo_fd, &finish_after_eof);
                if (request_result < 0) {
                    close(fifo_fd);
                    return -1;
                }
                if (request_result > 0) {
                    close(fifo_fd);
                    return 0;
                }
                continue;
            }

            fprintf(stderr, "Cannot read FIFO %s: %s\n", FIFO_PATH, strerror(errno));
            close(fifo_fd);
            return -1;
        }

        if (close(fifo_fd) != 0) {
            fprintf(stderr, "Cannot close FIFO %s: %s\n", FIFO_PATH, strerror(errno));
            return -1;
        }

        stats.messages++;

        if (finish_after_eof)
            break;
    }

    return 0;
}

int main(int argc, char **argv) {
    int daemon_mode = 0;
    int created_fifo = 0;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
        return 1;
    }

    if (argc == 2) {
        if (strcmp(argv[1], "-d") != 0) {
            fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
            return 1;
        }

        daemon_mode = 1;
    }

    if (daemon_mode && start_daemon_mode(0) != 0)
        return 1;
    if (setup_signals() != 0)
        return 1;
    if (prepare_fifo(&created_fifo) != 0)
        return 1;

    alarm(ALARM_SEC);
    if (run_server(&daemon_mode) != 0) {
        print_stats();

        if (created_fifo && unlink(FIFO_PATH) != 0)
            fprintf(stderr, "Cannot remove FIFO %s: %s\n", FIFO_PATH, strerror(errno));
        return 1;
    }

    if (created_fifo && unlink(FIFO_PATH) != 0) {
        fprintf(stderr, "Cannot remove FIFO %s: %s\n", FIFO_PATH, strerror(errno));
        return 1;
    }
    print_stats();

    printf("Log server stopped\n");

    return 0;
}
