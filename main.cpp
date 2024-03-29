#include <stdio.h>
#include <math.h>
#include <time.h>
#include <iostream>
#include <chrono>
#include <vector>

// fork
#include <sys/types.h>
#include <unistd.h>
// mmap
#include <sys/mman.h>
#include <sys/wait.h>

#include <sys/sysinfo.h>

#define WIDTH 1000
#define HEIGHT 1000
#define PRINT_DELAY_SECONDS 1

struct point {
    int x;
    int y;
};

static bool is_generated_point_in_circle() {
    /**
     * Generate a random point in the space and
     * return whether said point is inside the circle
    */

    point p;
    p.x = rand() % WIDTH;
    p.y = rand() % HEIGHT;

    long double distance_from_center = sqrt(pow(p.x - WIDTH / 2, 2) + pow(p.y - HEIGHT / 2, 2));

    return distance_from_center <= 500;
}

void child_compute(long long int *result) {
    /**
     * Executed in each child process, *result
     * points to a shared memory space when must be put
     * the total points count and total points in circle
     * count. This data will be then red by the parent
     * process.
    */
    srand(time(NULL));

    long long int *total_points = result + 0;
    long long int *circle_points = result + 1;

    for (;;) {
        int is_inside = is_generated_point_in_circle();
        
        *total_points += 1;

        if (is_inside == 1) {
            *circle_points += 1;
        }
    }

}

int main() {
    const int PROCESS_COUNT = get_nprocs();

    std::cout << "Running with " << PROCESS_COUNT << " threads" << std::endl;

    /**
     * Allocate a shared memory location that will be written
     * to by forked processes and red by the parent.
    */
    long long int *results = static_cast<long long int*>(mmap(
        NULL, sizeof (long long int) * PROCESS_COUNT * 2,
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1, 0
    ));

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<pid_t> child_pids = { };

    /**
     * Spawn childrens based on PROCESS_COUNT, each child will
     * call child_compute with a pointer to its shared memory location
     */
    for (size_t i = 0; i < PROCESS_COUNT; i++) {
        pid_t pid = fork();
        child_pids.push_back(pid);

        if (pid == -1) {
            printf("fork() error, unable to fork current process\n");
            exit(1);

        } else if (pid == 0) {  // Child
            child_compute(results + (i * sizeof (long long int) * 2));
            exit(0);

        }
    }

    long double pi = 0;

    /**
     * Parent code, read all memory written by each child and compute
     * the new value of pi every PRINT_DELAY_SECONDS
    */
    for (;;) {
        long long int total_points = 0;
        long long int circle_points = 0;

        for (size_t i = 0; i < PROCESS_COUNT; i++) {
            total_points += *(results + (i * sizeof (long long int) * 2));
            circle_points += *(results + (i * sizeof (long long int) * 2 + 1));
        }

        if (total_points >= 1e8) {
            break;
        }

        pi = (long double) circle_points / total_points * 4;
        // std::cout << total_points << std::endl;
        // sleep(PRINT_DELAY_SECONDS);
        sleep(0.01);
    }

    std::cout << "Pi: " << pi << std::endl;

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
    printf("Code took %d milliseconds with %d processes\n", duration, PROCESS_COUNT);

    munmap(results, sizeof (long long int) * PROCESS_COUNT * 2);

    for (pid_t pid : child_pids) {
        kill(pid, SIGKILL);
    }

    return 0;
}
