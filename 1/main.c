#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>
#include "libcoro.h"

#define MAX_FILES 10
#define MAX_NUMBERS_IN_FILE 10000
#define FILE_NAME_SIZE 20

struct env {
    short id;
    char file[FILE_NAME_SIZE];
    int* array;
    int size;
    float maxTime;
    uint64_t timeCheckPoint;
    int workTime;
    int yield_counter;
}; 

char file_queue[MAX_FILES][FILE_NAME_SIZE];
int arrays[MAX_FILES][MAX_NUMBERS_IN_FILE];
int sizes[MAX_FILES];
int queue_ind = 0;
int queue_size = 0;


// https://rextester.com/ABZ2569
uint64_t getTime() {
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	return (uint64_t)currentTime.tv_sec * 1000000 + (uint64_t)currentTime.tv_usec;
}

void swap(int* a, int* b) {
    int t = *a;
    *a = *b;
    *b = t;
}

int partition(struct env* coro_env, int start, int end) {
    int pivot = coro_env->array[end];
    int j = start - 1;
 
    for (int i = start; i <= end - 1; i++) {
        uint64_t ts = getTime();    
        if (ts - coro_env->timeCheckPoint > coro_env->maxTime) {
            coro_env->workTime += (int)(ts - coro_env->timeCheckPoint);
            coro_env->yield_counter++;
            coro_yield();
            coro_env->timeCheckPoint = getTime();
        }

        if (coro_env->array[i] < pivot) {
            j++;
            swap(&coro_env->array[j], &coro_env->array[i]);
        }
    }
    swap(&coro_env->array[j + 1], &coro_env->array[end]);
    return j + 1;
}


int quick_sort(struct env* enva, int start, int end) {
    if (start < end) {
        int idx = partition(enva, start, end);
        quick_sort(enva, start, idx - 1);
        quick_sort(enva, idx + 1, end);
    }
}

static int coroutine_func_f(void *context) {
	struct coro *this = coro_this();
	struct env* envirounment = context;

    printf("Корутина [%d]: Начинаю работу\n", envirounment->id);

    while (queue_ind < queue_size) {
        // Подготока данных кооутины для след. файла
        strcpy(envirounment->file, file_queue[queue_ind]);
        envirounment->size = 0;
        envirounment->array = arrays[queue_ind];
    
        FILE* file = fopen(envirounment->file, "r");
        int idx = queue_ind;
        queue_ind++;

        int size = 0;

        printf("Корутина [%d]: Взял файл %s\n", envirounment->id, envirounment->file);

        // Считываем данные из файла
        while (!feof(file)) {
            uint64_t ts = getTime();
            // if (ts - envirounment->timeCheckPoint > envirounment->maxTime) {
            //     envirounment->workTime += (int)(ts - envirounment->timeCheckPoint);
            //     envirounment->yield_counter++;
            //     coro_yield();
            //     envirounment->timeCheckPoint = getTime();
            // }
            fscanf(file, "%d", &envirounment->array[size]);
            size++;
        }

        envirounment->size = size;
        sizes[idx] = size;

        fclose(file);

        // Сортировка
        quick_sort(envirounment, 0, size - 1);
    }

    envirounment->workTime += (int)(getTime() - envirounment->timeCheckPoint);
    printf("Корутина [%d]: Завершаю работу\n", envirounment->id);

	return 0;
}

int main(int argc, char* argv[]) {
    uint64_t mainTimeStart = getTime();
    struct env envs[10];
    int t = atoi(argv[1]); // in microseconds
    int n = atoi(argv[2]);
    int finalIndex[10];

    for (int i = 0; i < MAX_FILES; i++) {
        sizes[i] = 0;
        finalIndex[i] = 0;
        for (int j = 0; j < MAX_NUMBERS_IN_FILE; j++) {
            arrays[i][j] = 0;
        }
    }

    // "Очередь" из файлов
    for (int i = 3; i < argc; i++) {
        strcpy(file_queue[queue_size], argv[i]);
        queue_size++;
    }

	coro_sched_init();

    // Запуск корутин
	for (int i = 0; i < n; i++) {
        envs[i].maxTime = t / n;
        envs[i].id = i;
        envs[i].yield_counter = 0;
        envs[i].timeCheckPoint = getTime();

		coro_new(coroutine_func_f, &envs[i]);
	}

	struct coro *c;
	while ((c = coro_sched_wait()) != NULL) {
		// printf("Finished %d\n", coro_status(c));
		coro_delete(c);
	}

    FILE* ans = fopen("out.txt", "w");

    // Слияние массивов в файл out.txt
    while(1) {
        int countFinished = 0;
        int smallest = 2147483647;
        int smallestIndex;
        for (int i = 0; i < queue_size; i++) {
            if (finalIndex[i] == sizes[i]) {
                countFinished++;
                continue;
            }

            // printf("%d ", arrays[i][finalIndex[i]]);
            
            if (arrays[i][finalIndex[i]] <= smallest) {
                smallest = arrays[i][finalIndex[i]];
                smallestIndex = i;
            }
        }
        if (countFinished == queue_size) break;

        fprintf(ans, "%d ", smallest);
        // printf("%d ", smallestIndex);
        finalIndex[smallestIndex]++;
    }

    printf("\nСуммарное время работы: %d мкс\n", (getTime() - mainTimeStart));
    for (int i = 0; i < n; i++) {
        printf("Корутина [%d]: Время работы %d мкс, Количество yield %d\n", i, envs[i].workTime, envs[i].yield_counter);
    }
    

    fclose(ans);
	return 0;
}