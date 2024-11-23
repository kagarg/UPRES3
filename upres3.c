#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
volatile sig_atomic_t flag = 0;

void signal_handler(int signum) {
    if (signum == SIGUSR1 ||signum == SIGUSR2) {
        flag = 1;
    }
}

int main() {
    int k1[2], k2[2]; // сюда программный канал будет возвращать два дескриптора файла (один на запись второй на чтение)
    pid_t pid1, pid2;
    signal(SIGUSR1, SIG_IGN);
    signal(SIGUSR2, SIG_IGN);
    if (pipe(k1) == -1 || pipe(k2) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    printf("Исходный процесс создал два программных канала К1 и К2\n");
    pid1 = fork(); // породили новый процесс р1
    if (pid1 < 0)
    { // проверяем создался ли процесс
        perror("fork (pid1)");
        exit(EXIT_FAILURE);
    }
    else if (pid1 == 0)
    { // Р1 процесс
        close(k1[0]); // закрываем конец для чтения первого канала, он нам не понадобится ни в Р1 ни в Р2
        printf("Создан процесс Р1\n");

        // создаем второй дочерний процесс внутри первого дочернего
        pid2 = fork();
        if (pid2 < 0)
        {
            perror("fork (pid2)");
            exit(EXIT_FAILURE);
        }
        else if (pid2 == 0)
        { // Р2 процесс
            printf("Создан процесс Р2\n");
            close(k2[0]);
            printf("Child: Waiting for signal from parent...\n");
            signal(SIGUSR1, signal_handler);
            pause();
            if (flag) { printf("Child: Received SIGUSR1 signal from parent\n"); }
            // Готовим данные и помещаем их в канал К2
            FILE *inp = fopen("in.txt", "r");
            if (inp == NULL) {
                perror("Unable to open input file");
                return 1;
            }
            char data[40];
            fgets(data, sizeof(data), inp);
            fgets(data, sizeof(data), inp);
            fclose(inp);
            int id = getpid();
            char input[44];
            char ID[4];
            sprintf(ID, "%i", id);
            for (int i = 0; i < 4; i++)
            {
                input[i] = ID[i];
            }
            for (int i = 0; i < 40; i++)
            {
                input[i + 4] = data[i];
            }
            write(k2[1], input, sizeof(data));
            printf("Процесс Р2 завершил посылку данных в канал К2\n");
            close(k2[1]);
            kill(getppid(), SIGUSR2); // отправляем сигнал процессу Р1
            printf("Процесс Р2 отправил сигнал P1\n");
            exit(0);
        }
        else
        { // процесс р1
            // Готовим данные и помещаем их в канал К1
            FILE *in = fopen("in.txt", "r");
            if (in == NULL) {
                perror("Unable to open input file");
                return 1;
            }
            char data[40], input[44], ID[4];
            fgets(data, sizeof(data), in);
           
            int id = getpid();
            
            int param = sprintf(ID, "%i", id);
            for (int i = 0; i < 4; i++)
            {
                input[i] = ID[i];
            }
            for (int i = 0; i < 40; i++)
            {
                input[i+4] = data[i];
            }
            write(k1[1], input, sizeof(input));
            printf("Процесс Р1 завершил посылку данных в канал К1\n");

            printf("Parent: Sending SIGUSR1 signal to child\n");
            kill(pid1, SIGUSR1);
            printf("Parent: Ready to receive SIGUSR2 signal\n");
            flag = 0;
            signal(SIGUSR2, signal_handler);
            pause();
            if (flag){printf("Parent: Received SIGUSR2 signal from child\n");}
            
            read(k2[0], input, sizeof(input));
            write(k1[1], input, sizeof(input));
            
            while (fgets(data, sizeof(data), in) != NULL) {}

            for (int i = 0; i < 4; i++)
            {
                input[i] = ID[i];
            }
            for (int i = 0; i < 40; i++)
            {
                input[i +4] = data[i];
            }
            write(k1[1], input, sizeof(input));

            wait(NULL); // Ожидаем завершения процесса P2
            close(k2[1]); // закрываем все оставшиеся концы каналов и файл
            fclose(in);
            close(k2[0]);
            close(k1[1]);
            exit(0);
        }
    }
    else
    { // Основной процесс
        close(k1[1]);
        printf("Основной процесс начинает обработку данных\n");
        // Чтение информации из канала К1 и печать
        char buffer[44];
        FILE *out = fopen("out.txt", "w");
        ssize_t bytesRead;
        while (bytesRead = read(k1[0], buffer, sizeof(buffer)))
        {
            if (bytesRead > 0)
            {
                buffer[bytesRead] = '\0';
                printf("Информация из канала К1: %s\n", buffer);
                fprintf(out, "%s\n", buffer);
            }
        }
        printf("Основной процесс завершил обработку данных\n");
        wait(NULL);
        close(k1[0]);
        close(k2[0]);
        close(k2[1]);
    }
    return 0;
}
