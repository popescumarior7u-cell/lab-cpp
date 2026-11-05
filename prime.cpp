#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <cstdio>

using namespace std;

const int N = 10;
const int LIMIT = 10000;
const int INTERVAL = 1000;

int inputPipe[N][2];
int outputPipe[N][2];

bool isPrime(int n) {
    if (n < 2) return false;

    for (int i = 2; i <= sqrt(n); i++) {
        if (n % i == 0)
            return false;
    }

    return true;
}

bool writeInt(int fd, int value) {
    const char* ptr = reinterpret_cast<const char*>(&value);
    size_t remaining = sizeof(value);

    while (remaining > 0) {
        ssize_t n = write(fd, ptr, remaining);
        if (n <= 0) return false;
        ptr += n;
        remaining -= n;
    }

    return true;
}

bool readInt(int fd, int& value) {
    char* ptr = reinterpret_cast<char*>(&value);
    size_t remaining = sizeof(value);

    while (remaining > 0) {
        ssize_t n = read(fd, ptr, remaining);
        if (n == 0) return false;
        if (n < 0) return false;
        ptr += n;
        remaining -= n;
    }

    return true;
}

int main() {
    pid_t pids[N];

    // Cream toate pipe-urile
    for (int i = 0; i < N; i++) {
        if (pipe(inputPipe[i]) == -1 ||
            pipe(outputPipe[i]) == -1) {
            perror("Eroare la crearea pipe-urilor");
            return 1;
        }
    }

    // Cream cele 10 procese copil
    for (int i = 0; i < N; i++) {
        pids[i] = fork();

        if (pids[i] < 0) {
            perror("Eroare fork");
            return 1;
        }

        if (pids[i] == 0) {
            // Proces copil

            for (int j = 0; j < N; j++) {
                close(inputPipe[j][1]);
                close(outputPipe[j][0]);

                if (j != i) {
                    close(inputPipe[j][0]);
                    close(outputPipe[j][1]);
                }
            }

            int start, end;

            if (!readInt(inputPipe[i][0], start) ||
                !readInt(inputPipe[i][0], end)) {
                exit(1);
            }

            close(inputPipe[i][0]);

            cerr << "Procesul " << i + 1
                 << " analizeaza intervalul "
                 << start << " - " << end << endl;

            for (int num = start; num <= end; num++) {
                if (isPrime(num)) {
                    if (!writeInt(outputPipe[i][1], num)) {
                        exit(1);
                    }
                }
            }

            close(outputPipe[i][1]);
            exit(0);
        }
    }

    // Procesul parinte
    for (int i = 0; i < N; i++) {
        close(inputPipe[i][0]);
        close(outputPipe[i][1]);
    }

    cout << "\n=== CAUTAREA NUMERELOR PRIME ===\n";
    cout << "Interval: 1 - " << LIMIT << endl;
    cout << "Numar procese: " << N << endl;

    // Trimitem intervalele catre procese
    for (int i = 0; i < N; i++) {
        int start = i * INTERVAL + 1;
        int end = (i + 1) * INTERVAL;

        writeInt(inputPipe[i][1], start);
        writeInt(inputPipe[i][1], end);

        close(inputPipe[i][1]);
    }

    // Primim rezultatele
    int total = 0;

    cout << "\n=== NUMERE PRIME GASITE ===\n";

    for (int i = 0; i < N; i++) {
        cout << "\nProcesul " << i + 1 << ": ";

        int number;

        while (readInt(outputPipe[i][0], number)) {
            cout << number << " ";
            total++;
        }

        close(outputPipe[i][0]);
        cout << endl;
    }

    // Asteptam terminarea proceselor
    for (int i = 0; i < N; i++) {
        waitpid(pids[i], nullptr, 0);
    }

    cout << "\n================================\n";
    cout << "Total numere prime: " << total << endl;
    cout << "Toate cele 10 procese au fost finalizate.\n";

    return 0;
}
