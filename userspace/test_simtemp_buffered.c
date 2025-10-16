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
    struct timespec start, end;
    double elapsed;

    fd = open("/dev/simtemp", O_RDONLY);
    if (fd < 0) {
        perror("Failed to open /dev/simtemp");
        return 1;
    }

    printf("Testing buffered temperature readings...\n");
    printf("Driver generates samples every 100ms automatically.\n");
    printf("This test reads as fast as possible from the buffer.\n\n");
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (i = 0; i < 20; i++) {
        if (read(fd, &sample, sizeof(sample)) != sizeof(sample)) {
            perror("Read failed");
            break;
        }

        printf("Sample %2d: %d.%03d°C  [0x%02x] ", 
               i + 1,
               sample.temp_mC / 1000, 
               abs(sample.temp_mC % 1000),
               sample.flags);
        
        if (sample.flags & 0x01)
            printf("NEW ");
        if (sample.flags & 0x02)
            printf("THRESH_EXCEEDED");
        printf("\n");
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + 
              (end.tv_nsec - start.tv_nsec) / 1000000000.0;

    printf("\nRead 20 samples in %.3f seconds\n", elapsed);
    printf("Average read rate: %.1f samples/sec\n", 20.0 / elapsed);

    close(fd);
    return 0;
}
