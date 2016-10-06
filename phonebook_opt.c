#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "phonebook_opt.h"
#include "debug.h"

entry *findName(char lastname[], entry *pHead)
{
    while (pHead != NULL) {
        if (strcmp(lastname, pHead->lastName) == 0)
            return pHead;
        dprintf("find string = %s\n", pHead->lastName);
        pHead = pHead->pNext;
    }
    return NULL;
}

thread_info_t *new_thread_info(char *ptr, char *eptr, int tid, int ntd,
                               entry *e_start, detail *d_start)
{
    thread_info_t *app = (thread_info_t*) malloc(sizeof(thread_info_t));

    app->map_start_ptr = ptr;
    app->map_end_ptr = eptr;
    app->tid = tid;
    app->nthread = ntd;
    app->entryStart = e_start;
    app->detailStart = d_start;

    app->pHead = app->pLast = app->entryStart;
    return app;
}

void append(void *arg)
{
    struct timespec start, end;
    double cpu_time;

    clock_gettime(CLOCK_REALTIME, &start);

    thread_info_t *app = (thread_info_t *) arg;

    int count = 0;
    entry *j = app->entryStart;
    detail *k = app->detailStart;
    for (char *i = app->map_start_ptr; i < app->map_end_ptr;
            i += MAX_LAST_NAME_SIZE * app->nthread,
            j += app->nthread,
            k += app->nthread, count++) {
        if(i == app->map_start_ptr) {
            app->pLast = j;
        } else {
            app->pLast->pNext = j;
            app->pLast = app->pLast->pNext;
        }
        app->pLast->dtl = k;

        app->pLast->lastName = i;
        dprintf("thread %d append string = %s\n",
                app->tid, app->pLast->lastName);
        app->pLast->pNext = NULL;
    }
    clock_gettime(CLOCK_REALTIME, &end);
    cpu_time = diff_in_second(start, end);

    dprintf("thread take %lf sec, count %d\n", cpu_time, count);

    pthread_exit(NULL);
}

void show_entry(entry *pHead)
{
    while (pHead != NULL) {
        printf("lastName = %s\n", pHead->lastName);
        pHead = pHead->pNext;
    }
}

static double diff_in_second(struct timespec t1, struct timespec t2)
{
    struct timespec diff;
    if (t2.tv_nsec-t1.tv_nsec < 0) {
        diff.tv_sec  = t2.tv_sec - t1.tv_sec - 1;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec + 1000000000;
    } else {
        diff.tv_sec  = t2.tv_sec - t1.tv_sec;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec;
    }
    return (diff.tv_sec + diff.tv_nsec / 1000000000.0);
}
