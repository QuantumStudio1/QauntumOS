#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void log_message(const char *message) {
    dprintf(STDERR_FILENO, "qauntum-init: %s\n", message);
}

static int mount_system(const char *source, const char *target, const char *type) {
    if (mkdir(target, 0755) < 0 && errno != EEXIST) {
        dprintf(STDERR_FILENO, "qauntum-init: mkdir %s: %s\n", target, strerror(errno));
        return -1;
    }
    if (mount(source, target, type, MS_NOSUID | MS_NOEXEC, NULL) < 0) {
        dprintf(STDERR_FILENO, "qauntum-init: mount %s: %s\n", target, strerror(errno));
        return -1;
    }
    return 0;
}

static void open_console(void) {
    int fd = open("/dev/console", O_RDWR);
    if (fd < 0) return;
    for (int i = 0; i < 3; ++i) (void)dup2(fd, i);
    if (fd > 2) close(fd);
}

static void read_boot_options(int *recovery, char *data_device, size_t capacity) {
    FILE *file = fopen("/proc/cmdline", "r");
    if (!file) return;
    char line[4096];
    if (fgets(line, sizeof(line), file)) {
        char *save = NULL;
        for (char *token = strtok_r(line, " \t\n", &save); token;
             token = strtok_r(NULL, " \t\n", &save)) {
            if (strcmp(token, "qauntum.recovery=1") == 0) *recovery = 1;
            if (strncmp(token, "qauntum.data=", 13) == 0 &&
                strncmp(token + 13, "/dev/", 5) == 0 &&
                strlen(token + 13) < capacity)
                strcpy(data_device, token + 13);
        }
    }
    fclose(file);
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--check") == 0) {
        puts("qauntum-init build check OK");
        return 0;
    }
    if (getpid() != 1) {
        fputs("qauntum-init must run as PID 1\n", stderr);
        return 1;
    }

    (void)mount_system("devtmpfs", "/dev", "devtmpfs");
    open_console();
    (void)mount_system("proc", "/proc", "proc");
    (void)mount_system("sysfs", "/sys", "sysfs");
    int recovery = 0;
    char data_device[128] = {0};
    read_boot_options(&recovery, data_device, sizeof(data_device));
    if (data_device[0]) {
        if (mount(data_device, "/var/lib/qauntumos", "ext4",
                  MS_NODEV | MS_NOSUID | MS_NOEXEC, NULL) == 0) {
            (void)mkdir("/var/lib/qauntumos/accounts", 0700);
            (void)setenv("QAUNTUM_PERSISTENT", "1", 1);
            log_message("persistent account data mounted");
        } else dprintf(STDERR_FILENO, "qauntum-init: data disk %s: %s\n",
                       data_device, strerror(errno));
    } else log_message("live account data is temporary");
    log_message(recovery ? "booted; starting recovery shell" :
                           "booted; starting account and lock screen");

    for (;;) {
        pid_t child = fork();
        if (child == 0) {
            (void)setsid();
            const char *program = recovery ? "/bin/sh" : "/bin/qauntum-session";
            execl(program, recovery ? "sh" : "qauntum-session", (char *)NULL);
            dprintf(STDERR_FILENO, "qauntum-init: %s: %s\n", program, strerror(errno));
            _exit(127);
        }
        if (child < 0) {
            log_message("fork failed; powering off");
            sync();
            reboot(RB_POWER_OFF);
            _exit(1);
        }
        int status;
        while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
        while (waitpid(-1, &status, WNOHANG) > 0) {}
        log_message("session exited; restarting in 2 seconds");
        sleep(2);
    }
}
