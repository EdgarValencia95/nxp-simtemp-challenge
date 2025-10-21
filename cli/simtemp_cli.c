/*
 * simtemp_cli.c - Command Line Interface for NXP Simulated Temperature Sensor
 * 
 * This CLI application reads temperature data from the NXP simtemp driver
 * and displays it in various formats with real-time monitoring capabilities.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <getopt.h>
#include <sys/ioctl.h>

/* Device path */
#define DEVICE_PATH "/dev/simtemp"

/* Sample flags - must match kernel driver */
#define SIMTEMP_FLAG_NEW_SAMPLE         0x01
#define SIMTEMP_FLAG_THRESHOLD_EXCEEDED 0x02

/* Sample structure - must match kernel driver */
struct simtemp_sample {
    uint64_t timestamp_ns;
    int32_t temp_mC;
    uint32_t flags;
} __attribute__((packed));

/* CLI configuration */
struct cli_config {
    int continuous;        /* Continuous mode flag */
    int samples;          /* Number of samples to read (0 = infinite) */
    int interval_ms;      /* Interval between samples in ms */
    char *format;         /* Output format: table, json, csv */
    int show_stats;       /* Show statistics */
    int verbose;          /* Verbose output */
    char *device_path;    /* Device path */
};

/* Statistics structure */
struct temp_stats {
    int32_t min_temp;
    int32_t max_temp;
    int64_t sum_temp;
    uint32_t count;
    uint32_t threshold_count;
};

/* Global flag for signal handling */
static volatile int keep_running = 1;

/**
 * Signal handler for clean exit
 */
static void signal_handler(int signum)
{
    (void)signum;
    keep_running = 0;
    printf("\n\nReceived interrupt signal. Exiting...\n");
}

/**
 * Initialize statistics
 */
static void stats_init(struct temp_stats *stats)
{
    stats->min_temp = INT32_MAX;
    stats->max_temp = INT32_MIN;
    stats->sum_temp = 0;
    stats->count = 0;
    stats->threshold_count = 0;
}

/**
 * Update statistics with new sample
 */
static void stats_update(struct temp_stats *stats, const struct simtemp_sample *sample)
{
    if (sample->temp_mC < stats->min_temp)
        stats->min_temp = sample->temp_mC;
    
    if (sample->temp_mC > stats->max_temp)
        stats->max_temp = sample->temp_mC;
    
    stats->sum_temp += sample->temp_mC;
    stats->count++;
    
    if (sample->flags & SIMTEMP_FLAG_THRESHOLD_EXCEEDED)
        stats->threshold_count++;
}

/**
 * Print statistics
 */
static void stats_print(const struct temp_stats *stats)
{
    if (stats->count == 0) {
        printf("\nNo samples collected.\n");
        return;
    }

    int32_t avg_temp = stats->sum_temp / stats->count;

    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║         Temperature Statistics         ║\n");
    printf("╠════════════════════════════════════════╣\n");
    printf("║ Total Samples:      %-18u ║\n", stats->count);
    printf("║ Min Temperature:    %6d.%03d°C       ║\n", 
           stats->min_temp / 1000, abs(stats->min_temp % 1000));
    printf("║ Max Temperature:    %6d.%03d°C       ║\n", 
           stats->max_temp / 1000, abs(stats->max_temp % 1000));
    printf("║ Avg Temperature:    %6d.%03d°C       ║\n", 
           avg_temp / 1000, abs(avg_temp % 1000));
    printf("║ Threshold Exceeded: %-18u ║\n", stats->threshold_count);
    printf("╚════════════════════════════════════════╝\n");
}

/**
 * Print sample in table format
 */
static void print_sample_table(const struct simtemp_sample *sample, uint32_t index, int verbose)
{
    /* Header on first sample */
    if (index == 1) {
        printf("\n");
        printf("╔═══════╦════════════════╦═══════════════════╦══════════════════════════╗\n");
        printf("║ Index ║  Temperature   ║      Flags        ║        Timestamp         ║\n");
        printf("╠═══════╬════════════════╬═══════════════════╬══════════════════════════╣\n");
    }

    /* Temperature with color coding */
    char temp_str[20];
    snprintf(temp_str, sizeof(temp_str), "%6d.%03d°C", 
             sample->temp_mC / 1000, abs(sample->temp_mC % 1000));

    /* Flags string */
    char flags_str[20] = "";
    if (sample->flags & SIMTEMP_FLAG_NEW_SAMPLE)
        strcat(flags_str, "NEW ");
    if (sample->flags & SIMTEMP_FLAG_THRESHOLD_EXCEEDED)
        strcat(flags_str, "⚠ THRESH");

    /* Add color for threshold exceeded */
    if (sample->flags & SIMTEMP_FLAG_THRESHOLD_EXCEEDED) {
        printf("║ %5u ║ \033[1;31m%-14s\033[0m ║ %-17s ║", index, temp_str, flags_str);
    } else {
        printf("║ %5u ║ %-14s ║ %-17s ║", index, temp_str, flags_str);
    }

    if (verbose) {
        printf(" %16lu ns ║\n", (unsigned long)sample->timestamp_ns);
    } else {
        /* Show relative time */
        static uint64_t first_timestamp = 0;
        if (first_timestamp == 0)
            first_timestamp = sample->timestamp_ns;
        uint64_t elapsed_ms = (sample->timestamp_ns - first_timestamp) / 1000000;
        printf(" +%-14lu ms      ║\n", (unsigned long)elapsed_ms);
    }
}

/**
 * Print sample in JSON format
 */
static void print_sample_json(const struct simtemp_sample *sample, uint32_t index, int is_first, int is_last)
{
    if (is_first)
        printf("[\n");

    printf("  {\n");
    printf("    \"index\": %u,\n", index);
    printf("    \"temperature_C\": %.3f,\n", sample->temp_mC / 1000.0);
    printf("    \"temperature_mC\": %d,\n", sample->temp_mC);
    printf("    \"timestamp_ns\": %lu,\n", (unsigned long)sample->timestamp_ns);
    printf("    \"flags\": {\n");
    printf("      \"new_sample\": %s,\n", 
           (sample->flags & SIMTEMP_FLAG_NEW_SAMPLE) ? "true" : "false");
    printf("      \"threshold_exceeded\": %s\n", 
           (sample->flags & SIMTEMP_FLAG_THRESHOLD_EXCEEDED) ? "true" : "false");
    printf("    }\n");
    printf("  }%s\n", is_last ? "" : ",");

    if (is_last)
        printf("]\n");
}

/**
 * Print sample in CSV format
 */
static void print_sample_csv(const struct simtemp_sample *sample, uint32_t index, int is_first)
{
    if (is_first)
        printf("Index,Temperature_C,Temperature_mC,Timestamp_ns,New_Sample,Threshold_Exceeded\n");

    printf("%u,%.3f,%d,%lu,%d,%d\n",
           index,
           sample->temp_mC / 1000.0,
           sample->temp_mC,
           (unsigned long)sample->timestamp_ns,
           (sample->flags & SIMTEMP_FLAG_NEW_SAMPLE) ? 1 : 0,
           (sample->flags & SIMTEMP_FLAG_THRESHOLD_EXCEEDED) ? 1 : 0);
}

/**
 * Print usage information
 */
static void print_usage(const char *prog_name)
{
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("NXP Simulated Temperature Sensor CLI\n\n");
    printf("Options:\n");
    printf("  -c, --continuous         Run in continuous mode (until Ctrl+C)\n");
    printf("  -n, --samples=N          Read N samples (default: 10)\n");
    printf("  -i, --interval=MS        Interval between samples in ms (default: 0)\n");
    printf("  -f, --format=FORMAT      Output format: table, json, csv (default: table)\n");
    printf("  -s, --stats              Show statistics at the end\n");
    printf("  -v, --verbose            Verbose output\n");
    printf("  -d, --device=PATH        Device path (default: /dev/simtemp)\n");
    printf("  -h, --help               Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -n 20                      # Read 20 samples\n", prog_name);
    printf("  %s -c -s                      # Continuous mode with stats\n", prog_name);
    printf("  %s -n 100 -f json             # 100 samples in JSON format\n", prog_name);
    printf("  %s -c -i 500                  # Continuous with 500ms interval\n", prog_name);
    printf("\n");
}

/**
 * Main function
 */
int main(int argc, char *argv[])
{
    struct cli_config config = {
        .continuous = 0,
        .samples = 10,
        .interval_ms = 0,
        .format = "table",
        .show_stats = 0,
        .verbose = 0,
        .device_path = DEVICE_PATH,
    };

    struct temp_stats stats;
    int fd;
    struct simtemp_sample sample;
    uint32_t sample_index = 0;
    struct pollfd fds[1];

    /* Parse command line arguments */
    static struct option long_options[] = {
        {"continuous", no_argument,       0, 'c'},
        {"samples",    required_argument, 0, 'n'},
        {"interval",   required_argument, 0, 'i'},
        {"format",     required_argument, 0, 'f'},
        {"stats",      no_argument,       0, 's'},
        {"verbose",    no_argument,       0, 'v'},
        {"device",     required_argument, 0, 'd'},
        {"help",       no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "cn:i:f:svd:h", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'c':
            config.continuous = 1;
            config.samples = 0; /* Infinite */
            break;
        case 'n':
            config.samples = atoi(optarg);
            if (config.samples <= 0) {
                fprintf(stderr, "Error: Invalid sample count\n");
                return 1;
            }
            break;
        case 'i':
            config.interval_ms = atoi(optarg);
            if (config.interval_ms < 0) {
                fprintf(stderr, "Error: Invalid interval\n");
                return 1;
            }
            break;
        case 'f':
            config.format = optarg;
            if (strcmp(config.format, "table") != 0 &&
                strcmp(config.format, "json") != 0 &&
                strcmp(config.format, "csv") != 0) {
                fprintf(stderr, "Error: Invalid format. Use: table, json, or csv\n");
                return 1;
            }
            break;
        case 's':
            config.show_stats = 1;
            break;
        case 'v':
            config.verbose = 1;
            break;
        case 'd':
            config.device_path = optarg;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    /* Setup signal handler for clean exit */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialize statistics */
    stats_init(&stats);

    /* Open device */
    fd = open(config.device_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("Failed to open device");
        fprintf(stderr, "Make sure the kernel module is loaded and you have permissions.\n");
        fprintf(stderr, "Try: sudo %s\n", argv[0]);
        return 1;
    }

    if (config.verbose) {
        printf("Device opened: %s\n", config.device_path);
        printf("Mode: %s\n", config.continuous ? "Continuous" : "Fixed samples");
        if (!config.continuous)
            printf("Samples: %d\n", config.samples);
        printf("Format: %s\n", config.format);
        printf("\n");
    }

    /* Setup poll structure */
    fds[0].fd = fd;
    fds[0].events = POLLIN;

    /* Main reading loop */
    while (keep_running) {
        /* Check if we've read enough samples */
        if (!config.continuous && config.samples > 0 && sample_index >= (uint32_t)config.samples)
            break;

        /* Wait for data with poll (1 second timeout) */
        int ret = poll(fds, 1, 1000);
        
        if (ret < 0) {
            if (errno == EINTR)
                break; /* Interrupted by signal */
            perror("poll failed");
            break;
        } else if (ret == 0) {
            /* Timeout */
            if (config.verbose)
                printf("Waiting for data...\n");
            continue;
        }

        /* Data available, read it */
        if (fds[0].revents & POLLIN) {
            ssize_t bytes = read(fd, &sample, sizeof(sample));
            
            if (bytes == sizeof(sample)) {
                sample_index++;
                
                /* Update statistics */
                if (config.show_stats)
                    stats_update(&stats, &sample);
                
                /* Print sample based on format */
                if (strcmp(config.format, "table") == 0) {
                    print_sample_table(&sample, sample_index, config.verbose);
                } else if (strcmp(config.format, "json") == 0) {
                    int is_first = (sample_index == 1);
                    int is_last = (!config.continuous && sample_index == (uint32_t)config.samples);
                    print_sample_json(&sample, sample_index, is_first, is_last);
                } else if (strcmp(config.format, "csv") == 0) {
                    print_sample_csv(&sample, sample_index, (sample_index == 1));
                }
                
                /* Interval delay if specified */
                if (config.interval_ms > 0)
                    usleep(config.interval_ms * 1000);
                
            } else if (bytes < 0 && errno != EAGAIN) {
                perror("read failed");
                break;
            }
        }

        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "Error: Device error or disconnected\n");
            break;
        }
    }

    /* Print table footer */
    if (strcmp(config.format, "table") == 0 && sample_index > 0) {
        printf("╚═══════╩════════════════╩═══════════════════╩══════════════════════════╝\n");
    }

    /* Print statistics if requested */
    if (config.show_stats && sample_index > 0)
        stats_print(&stats);

    /* Cleanup */
    close(fd);

    if (config.verbose)
        printf("\nTotal samples read: %u\n", sample_index);

    return 0;
}
