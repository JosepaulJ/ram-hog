/*
 * RAM Hog - A cross-platform memory allocation utility
 * 
 * This program allocates memory in fixed-size chunks until either memory
 * is exhausted or a configurable maximum limit is reached. It holds onto
 * allocated memory indefinitely until manually terminated.
 * 
 * Compile with: gcc -O2 ramhog.c -o ramhog
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

/* Default configuration */
#define DEFAULT_CHUNK_SIZE_MB 100
#define DEFAULT_MAX_ALLOCATION_MB 0  /* 0 means unlimited */
#define GENTLE_DELAY_MS 100
#define MB_TO_BYTES(mb) ((size_t)(mb) * 1024 * 1024)
#define BYTES_TO_MB(bytes) ((double)(bytes) / (1024.0 * 1024.0))

/* Allocation speed modes */
typedef enum {
    SPEED_AGGRESSIVE = 0,
    SPEED_GENTLE = 1
} speed_mode_t;

/* Global configuration */
typedef struct {
    size_t chunk_size_bytes;
    size_t max_allocation_bytes;
    speed_mode_t speed_mode;
} config_t;

/* Global state */
typedef struct {
    void **memory_chunks;      /* Array of allocated chunk pointers */
    size_t chunks_allocated;   /* Number of chunks allocated */
    size_t chunks_capacity;    /* Capacity of the chunks array */
    size_t total_allocated;    /* Total bytes allocated */
    int running;               /* Flag to control main loop */
} state_t;

/* Global variables */
static config_t g_config;
static state_t g_state = {0};

/* Function prototypes */
void print_usage(const char *program_name);
int parse_arguments(int argc, char *argv[], config_t *config);
void signal_handler(int signum);
void setup_signal_handling(void);
int expand_chunks_array(void);
int allocate_chunk(void);
void cleanup_and_exit(int exit_code);
void cross_platform_sleep_ms(int milliseconds);
size_t parse_size_with_unit(const char *str);

/* Print usage information */
void print_usage(const char *program_name) {
    printf("RAM Hog - Memory allocation utility\n\n");
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -c, --chunk-size SIZE    Chunk size in MB (default: %d)\n", DEFAULT_CHUNK_SIZE_MB);
    printf("  -m, --max-alloc SIZE     Maximum allocation (MB/GB, 0=unlimited, default: %d)\n", DEFAULT_MAX_ALLOCATION_MB);
    printf("  -s, --speed MODE         Allocation speed: aggressive|gentle (default: aggressive)\n");
    printf("  -h, --help               Show this help message\n\n");
    printf("Size format examples:\n");
    printf("  100      - 100 MB\n");
    printf("  2G       - 2 GB\n");
    printf("  1024M    - 1024 MB\n\n");
    printf("Speed modes:\n");
    printf("  aggressive - Allocate as fast as possible\n");
    printf("  gentle     - Allocate with %dms delay between chunks\n\n", GENTLE_DELAY_MS);
    printf("The program will run until manually terminated (Ctrl+C).\n");
}

/* Parse size string with optional unit (M/G) */
size_t parse_size_with_unit(const char *str) {
    char *endptr;
    double value = strtod(str, &endptr);
    
    if (value < 0) {
        return 0;
    }
    
    /* Check for unit suffix */
    if (*endptr != '\0') {
        char unit = *endptr;
        if (unit == 'G' || unit == 'g') {
            value *= 1024;  /* Convert GB to MB */
        } else if (unit == 'M' || unit == 'm') {
            /* Already in MB, no conversion needed */
        } else {
            /* Invalid unit */
            return 0;
        }
    }
    
    return (size_t)value;
}

/* Parse command line arguments */
int parse_arguments(int argc, char *argv[], config_t *config) {
    /* Set defaults */
    config->chunk_size_bytes = MB_TO_BYTES(DEFAULT_CHUNK_SIZE_MB);
    config->max_allocation_bytes = MB_TO_BYTES(DEFAULT_MAX_ALLOCATION_MB);
    config->speed_mode = SPEED_AGGRESSIVE;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--chunk-size") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
                return -1;
            }
            size_t chunk_mb = parse_size_with_unit(argv[++i]);
            if (chunk_mb == 0) {
                fprintf(stderr, "Error: Invalid chunk size '%s'\n", argv[i]);
                return -1;
            }
            config->chunk_size_bytes = MB_TO_BYTES(chunk_mb);
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--max-alloc") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
                return -1;
            }
            size_t max_mb = parse_size_with_unit(argv[++i]);
            config->max_allocation_bytes = MB_TO_BYTES(max_mb);
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--speed") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
                return -1;
            }
            i++;
            if (strcmp(argv[i], "aggressive") == 0) {
                config->speed_mode = SPEED_AGGRESSIVE;
            } else if (strcmp(argv[i], "gentle") == 0) {
                config->speed_mode = SPEED_GENTLE;
            } else {
                fprintf(stderr, "Error: Invalid speed mode '%s'. Use 'aggressive' or 'gentle'\n", argv[i]);
                return -1;
            }
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            return -1;
        }
    }
    
    return 1;  /* Success */
}

/* Signal handler for graceful shutdown */
void signal_handler(int signum) {
    printf("\nReceived signal %d. Shutting down gracefully...\n", signum);
    g_state.running = 0;
}

/* Setup signal handling for graceful shutdown */
void setup_signal_handling(void) {
    signal(SIGINT, signal_handler);   /* Ctrl+C */
    signal(SIGTERM, signal_handler);  /* Termination request */
}

/* Cross-platform sleep function */
void cross_platform_sleep_ms(int milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);  /* usleep takes microseconds */
#endif
}

/* Expand the chunks array when it's full */
int expand_chunks_array(void) {
    size_t new_capacity = g_state.chunks_capacity == 0 ? 16 : g_state.chunks_capacity * 2;
    void **new_array = realloc(g_state.memory_chunks, new_capacity * sizeof(void*));
    
    if (new_array == NULL) {
        fprintf(stderr, "Error: Failed to expand chunks array\n");
        return 0;
    }
    
    g_state.memory_chunks = new_array;
    g_state.chunks_capacity = new_capacity;
    return 1;
}

/* Allocate a single chunk of memory */
int allocate_chunk(void) {
    /* Check if we've reached the maximum allocation limit */
    if (g_config.max_allocation_bytes > 0 && 
        g_state.total_allocated + g_config.chunk_size_bytes > g_config.max_allocation_bytes) {
        printf("Reached maximum allocation limit of %.2f MB. Stopping allocation.\n", 
               BYTES_TO_MB(g_config.max_allocation_bytes));
        return 0;
    }
    
    /* Expand chunks array if needed */
    if (g_state.chunks_allocated >= g_state.chunks_capacity) {
        if (!expand_chunks_array()) {
            return 0;
        }
    }
    
    /* Allocate the memory chunk */
    void *chunk = malloc(g_config.chunk_size_bytes);
    if (chunk == NULL) {
        printf("Memory allocation failed after %.2f MB. System memory exhausted.\n", 
               BYTES_TO_MB(g_state.total_allocated));
        return 0;
    }
    
    /* Touch the memory to ensure it's actually allocated by the OS */
    memset(chunk, 0xAA, g_config.chunk_size_bytes);
    
    /* Store the chunk pointer */
    g_state.memory_chunks[g_state.chunks_allocated] = chunk;
    g_state.chunks_allocated++;
    g_state.total_allocated += g_config.chunk_size_bytes;
    
    /* Log progress */
    printf("Allocated %.2f MB so far (chunk #%zu)\n", 
           BYTES_TO_MB(g_state.total_allocated), g_state.chunks_allocated);
    
    return 1;
}

/* Cleanup allocated memory and exit */
void cleanup_and_exit(int exit_code) {
    printf("\nCleaning up...\n");
    printf("Total memory allocated: %.2f MB in %zu chunks\n", 
           BYTES_TO_MB(g_state.total_allocated), g_state.chunks_allocated);
    
    /* Free all allocated chunks */
    for (size_t i = 0; i < g_state.chunks_allocated; i++) {
        free(g_state.memory_chunks[i]);
    }
    
    /* Free the chunks array */
    free(g_state.memory_chunks);
    
    printf("Cleanup complete. Goodbye!\n");
    exit(exit_code);
}

/* Main function */
int main(int argc, char *argv[]) {
    printf("RAM Hog - Memory allocation utility\n");
    printf("===================================\n\n");
    
    /* Parse command line arguments */
    int parse_result = parse_arguments(argc, argv, &g_config);
    if (parse_result <= 0) {
        return parse_result == 0 ? 0 : 1;  /* 0 for help, 1 for error */
    }
    
    /* Display configuration */
    printf("Configuration:\n");
    printf("  Chunk size: %.2f MB\n", BYTES_TO_MB(g_config.chunk_size_bytes));
    if (g_config.max_allocation_bytes > 0) {
        printf("  Maximum allocation: %.2f MB\n", BYTES_TO_MB(g_config.max_allocation_bytes));
    } else {
        printf("  Maximum allocation: Unlimited\n");
    }
    printf("  Speed mode: %s\n", g_config.speed_mode == SPEED_AGGRESSIVE ? "Aggressive" : "Gentle");
    printf("\nStarting memory allocation...\n");
    printf("Press Ctrl+C to stop and exit gracefully.\n\n");
    
    /* Setup signal handling */
    setup_signal_handling();
    
    /* Initialize state */
    g_state.running = 1;
    
    /* Main allocation loop */
    while (g_state.running) {
        if (!allocate_chunk()) {
            /* Allocation failed or limit reached */
            break;
        }
        
        /* Apply delay for gentle mode */
        if (g_config.speed_mode == SPEED_GENTLE) {
            cross_platform_sleep_ms(GENTLE_DELAY_MS);
        }
    }
    
    /* Keep the program running until interrupted if we stopped due to limits */
    if (g_state.running) {
        printf("\nAllocation complete. Holding memory until interrupted...\n");
        printf("Press Ctrl+C to exit.\n");
        
        while (g_state.running) {
            cross_platform_sleep_ms(1000);  /* Sleep for 1 second */
        }
    }
    
    /* Cleanup and exit */
    cleanup_and_exit(0);
    
    return 0;  /* This line should never be reached */
}