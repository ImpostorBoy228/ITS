// itsd.c - ITS daemon: periodically downloads finals.all and regenerates offset.dat
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <curl/curl.h>

static char *config_url = NULL;
static long config_timeout = 30;
static int config_interval = 3600;
static const char *CONFIG_FILE = "itsd.conf";

static void parse_config(void) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) { perror("itsd.conf"); exit(1); }
    char line[1024];

    if (!fgets(line, sizeof(line), f)) { fprintf(stderr, "itsd.conf: expected URL on line 1\n"); exit(1); }
    line[strcspn(line, "\r\n")] = 0;
    if (strlen(line) == 0) { fprintf(stderr, "itsd.conf: line 1 (URL) is empty\n"); exit(1); }
    config_url = strdup(line);

    if (!fgets(line, sizeof(line), f)) { fprintf(stderr, "itsd.conf: expected timeout on line 2\n"); exit(1); }
    config_timeout = atol(line);
    if (config_timeout <= 0) config_timeout = 30;

    if (!fgets(line, sizeof(line), f)) { fprintf(stderr, "itsd.conf: expected interval on line 3\n"); exit(1); }
    config_interval = atoi(line);
    if (config_interval <= 0) config_interval = 3600;

    fclose(f);
}

static int download_finals(void) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;
    FILE *f = fopen(FINALS_FILE, "wb");
    if (!f) { curl_easy_cleanup(curl); return -1; }
    curl_easy_setopt(curl, CURLOPT_URL, config_url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, config_timeout);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    CURLcode res = curl_easy_perform(curl);
    fclose(f);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK) ? 0 : -1;
}

static void do_update(void) {
    if (download_finals() != 0) {
        // download failed; keep old finals.all
    }
    double offset = compute_offset();
    if (offset < 0.0) return;
    FILE *f = fopen(OFFSET_FILE, "w");
    if (f) {
        fprintf(f, "%.6f\n", offset);
        fclose(f);
    }
}

static void print_timestamp(void) {
    FILE *fp = popen("./itstime -h 2>/dev/null || itstime -h 2>/dev/null", "r");
    if (fp) {
        char buf[64];
        if (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\r\n")] = 0;
            printf("%s  finals.all & offset.dat updated\n", buf);
            fflush(stdout);
        }
        pclose(fp);
    }
}

static void daemonize(void) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid > 0) exit(0);
    if (setsid() < 0) { perror("setsid"); exit(1); }
    signal(SIGCHLD, SIG_IGN);
    close(0); close(1); close(2);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);
}

int main(int argc, char **argv) {
    int daemon_mode = 0;
    if (argc == 2 && strcmp(argv[1], "-d") == 0) daemon_mode = 1;
    else if (argc > 1) { fprintf(stderr, "Usage: %s [-d]\n", argv[0]); exit(1); }

    parse_config();

    if (daemon_mode) daemonize();

    // initial run
    do_update();
    if (!daemon_mode) print_timestamp();

    while (1) {
        sleep(config_interval);
        do_update();
        if (!daemon_mode) print_timestamp();
    }
}
