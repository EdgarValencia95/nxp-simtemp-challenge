#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

struct simtemp_sample {
    uint64_t timestamp_ns;
    int32_t temp_mC;
    uint32_t flags;
} __attribute__((packed));

int main() {
    int fd;
    struct simtemp_sample sample;
    int i;

    fd = open("/dev/simtemp", O_RDONLY);
    if (fd < 0) {
        perror("Failed to open /dev/simtemp");
        return 1;
    }

    printf("Reading 10 temperature samples...\n\n");
    
    for (i = 0; i < 10; i++) {
        if (read(fd, &sample, sizeof(sample)) != sizeof(sample)) {
            perror("Read failed");
            break;
        }

        printf("Sample %d:\n", i + 1);
        printf("  Temperature: %d.%03d°C\n", 
               sample.temp_mC / 1000, 
               abs(sample.temp_mC % 1000));
        printf("  Timestamp: %lu ns\n", (unsigned long)sample.timestamp_ns);
        printf("  Flags: 0x%02x", sample.flags);
        
        if (sample.flags & 0x01)
            printf(" [NEW_SAMPLE]");
        if (sample.flags & 0x02)
            printf(" [THRESHOLD_EXCEEDED]");
        printf("\n\n");

        usleep(100000);
    }

    close(fd);
    return 0;
}
