
#include <windows.h>
#include <iostream>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace std;

const int N = 10;
const int INTERVAL = 1000;
const int LIMIT = 10000;

// Scrierea tuturor octetilor intr-un pipe
bool writeAll(HANDLE pipe, const void* data, DWORD size) {
    const char* ptr = static_cast<const char*>(data);

    while (size > 0) {
        DWORD written = 0;

        if (!WriteFile(pipe, ptr, size, &written, nullptr) ||
            written == 0) {
            return false;
        }

        ptr += written;
        size -= written;
    }

    return true;
}

// Citirea unui numar intreg din pipe
bool readInt(HANDLE pipe, int& value, bool& eof) {
    char* ptr = reinterpret_cast<char*>(&value);
    DWORD total = 0;

    eof = false;

    while (total < sizeof(value)) {
        DWORD received = 0;

        BOOL result = ReadFile(
            pipe,
            ptr + total,
            sizeof(value) - total,
            &received,
            nullptr
        );

        if (!result) {
            if (GetLastError() == ERROR_BROKEN_PIPE && total == 0) {
                eof = true;
                return true;
            }

            return false;
        }

        if (received == 0) {
            eof = true;
            return total == 0;
        }

        total += received;
    }

    return true;
}

// Verificarea numerelor prime
bool isPrime(int n) {
    if (n < 2) return false;

    for (int i = 2; i <= sqrt(n); i++) {
        if (n % i == 0)
            return false;
    }

    return true;
}

// Codul executat de fiecare proces copil
int childProcess() {
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);

    int start, end;
    bool eof = false;

    if (!readInt(input, start, eof) ||
        !readInt(input, end, eof)) {
        return 1;
    }

    for (int number = start; number <= end; number++) {
        if (isPrime(number)) {
            if (!writeAll(output, &number, sizeof(number))) {
                return 1;
            }
        }
    }

    return 0;
}

// Procesul principal
int parentProcess() {
    HANDLE processes[N];
    HANDLE inputWrite[N];
    HANDLE outputRead[N];

    STARTUPINFOA startupInfo[N];
    PROCESS_INFORMATION processInfo[N];

    char exePath[MAX_PATH];

    DWORD pathLength = GetModuleFileNameA(
        nullptr, exePath, MAX_PATH
    );

    if (pathLength == 0 || pathLength >= MAX_PATH) {
        cerr << "Eroare la identificarea executabilului.\n";
        return 1;
    }

    SECURITY_ATTRIBUTES security;
    security.nLength = sizeof(security);
    security.lpSecurityDescriptor = nullptr;
    security.bInheritHandle = TRUE;

    cout << "=== CAUTAREA NUMERELOR PRIME ===\n";
    cout << "Interval: 1 - " << LIMIT << endl;
    cout << "Numar procese: " << N << endl;

    // Crearea pipe-urilor si a proceselor copil
    for (int i = 0; i < N; i++) {
        HANDLE childInputRead, parentInputWrite;
        HANDLE parentOutputRead, childOutputWrite;

        if (!CreatePipe(
            &childInputRead,
            &parentInputWrite,
            &security, 0
        )) {
            cerr << "Eroare CreatePipe input.\n";
            return 1;
        }

        if (!CreatePipe(
            &parentOutputRead,
            &childOutputWrite,
            &security, 0
        )) {
            cerr << "Eroare CreatePipe output.\n";
            return 1;
        }

        // Parintele nu trebuie sa transmita aceste capete
        // mostenite mai departe altor procese.
        SetHandleInformation(
            parentInputWrite, HANDLE_FLAG_INHERIT, 0
        );

        SetHandleInformation(
            parentOutputRead, HANDLE_FLAG_INHERIT, 0
        );

        ZeroMemory(&startupInfo[i], sizeof(startupInfo[i]));
        startupInfo[i].cb = sizeof(startupInfo[i]);
        startupInfo[i].dwFlags = STARTF_USESTDHANDLES;
        startupInfo[i].hStdInput = childInputRead;
        startupInfo[i].hStdOutput = childOutputWrite;
        startupInfo[i].hStdError = GetStdHandle(STD_ERROR_HANDLE);

        ZeroMemory(&processInfo[i], sizeof(processInfo[i]));

        char commandLine[MAX_PATH + 32];

        snprintf(
            commandLine,
            sizeof(commandLine),
            "\"%s\" --child",
            exePath
        );

        BOOL created = CreateProcessA(
            exePath,
            commandLine,
            nullptr,
            nullptr,
            TRUE,
            0,
            nullptr,
            nullptr,
            &startupInfo[i],
            &processInfo[i]
        );

        // Parintele inchide capetele destinate copilului
        CloseHandle(childInputRead);
        CloseHandle(childOutputWrite);

        if (!created) {
            cerr << "Eroare CreateProcess: "
                << GetLastError() << endl;

            CloseHandle(parentInputWrite);
            CloseHandle(parentOutputRead);
            return 1;
        }

        inputWrite[i] = parentInputWrite;
        outputRead[i] = parentOutputRead;
        processes[i] = processInfo[i].hProcess;

        CloseHandle(processInfo[i].hThread);

        cout << "Procesul " << i + 1
            << " a fost creat.\n";
    }

    // Trimiterea intervalelor catre procese
    for (int i = 0; i < N; i++) {
        int start = i * INTERVAL + 1;
        int end = (i + 1) * INTERVAL;

        writeAll(inputWrite[i], &start, sizeof(start));
        writeAll(inputWrite[i], &end, sizeof(end));

        CloseHandle(inputWrite[i]);

        cout << "Procesul " << i + 1
            << " primeste intervalul "
            << start << " - " << end << endl;
    }

    // Receptionarea rezultatelor
    int total = 0;

    cout << "\n=== NUMERE PRIME GASITE ===\n";

    for (int i = 0; i < N; i++) {
        cout << "\nProcesul " << i + 1 << ": ";

        int number;
        bool eof = false;

        while (true) {
            if (!readInt(outputRead[i], number, eof)) {
                cerr << "\nEroare la citirea pipe-ului.\n";
                CloseHandle(outputRead[i]);
                return 1;
            }

            if (eof)
                break;

            cout << number << " ";
            total++;
        }

        CloseHandle(outputRead[i]);
        cout << endl;
    }

    // Asteptarea terminarii proceselor
    for (int i = 0; i < N; i++) {
        WaitForSingleObject(processes[i], INFINITE);
        CloseHandle(processes[i]);
    }

    cout << "\n================================\n";
    cout << "Total numere prime: " << total << endl;
    cout << "Toate cele 10 procese au fost finalizate.\n";

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "--child") == 0) {
        return childProcess();
    }

    return parentProcess();
}
