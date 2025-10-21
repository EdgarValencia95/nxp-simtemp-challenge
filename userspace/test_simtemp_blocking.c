#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

struct simtemp_sample {
    uint64_t timestamp_ns;
    int32_t temp_mC;
    uint32_t flags;
} __attribute__((packed));

int main() {
    int fd;
    struct simtemp_sample sample;
    int i;

    printf("=== Testing blocking read ===\n\n");
    printf("Opening device in BLOCKING mode...\n");

    /* Open WITHOUT O_NONBLOCK for blocking reads */
    fd = open("/dev/simtemp", O_RDONLY);
    if (fd < 0) {
        perror("Failed to open /dev/simtemp");
        return 1;
    }

    printf("Reading 10 samples (blocking until data available)...\n\n");

    for (i = 0; i < 10; i++) {
        printf("Reading sample %d... ", i + 1);
        fflush(stdout);

        /* This will block if no data available */
        if (read(fd, &sample, sizeof(sample)) != sizeof(sample)) {
            perror("\nRead failed");
            break;
        }

        printf("Got: %d.%03d°C [0x%02x]\n",
               sample.temp_mC / 1000,
               abs(sample.temp_mC % 1000),
               sample.flags);

        /* No delay needed - blocking read waits for new data */
    }

    close(fd);
    printf("\nTest completed.\n");
    return 0;
}
