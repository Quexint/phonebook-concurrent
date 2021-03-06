#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <omp.h>

#include IMPL

#define DICT_FILE "./dictionary/words_shuffle.txt"

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

#ifndef OPT
struct info_t {
    int num_words;
    double cpu_time1, cpu_time2;
    entry *pHead;
    char **test_lastname;
};
#else
struct info_t {
    int num_words;
    double cpu_time1, cpu_time2;
    entry *pHead;
    entry *entry_pool;
    detail *detail_pool;
    pthread_t *tid;
    thread_info_t **thread_info;
    char *map;
    off_t fs;
    char **test_lastname;
};
#endif

void input(struct info_t *var);
void make_test_array(struct info_t *var);
void search(struct info_t *var);
void output_execution_time(struct info_t *var);
void output_verified_files(struct info_t *var);
void cleanup(struct info_t *var);

int main(int argc, char *argv[])
{
    struct info_t var;

    input(&var);

    /* the givn last name to find */
    char input[MAX_LAST_NAME_SIZE] = "zyxel";
    assert(findName(input, var.pHead) &&
           "Did you implement findName() in " IMPL "?");
    assert(0 == strcmp(findName(input, var.pHead)->lastName, "zyxel"));

    make_test_array(&var);
    search(&var);
    output_execution_time(&var);
    output_verified_files(&var);
    cleanup(&var);
    return 0;
}

void input(struct info_t *var)
{
    struct timespec start, end;

    var->num_words = 0;

    /* Allocate the variables. */
#ifndef OPT
    FILE *fp;
    int i = 0;
    char line[MAX_LAST_NAME_SIZE];
#else
    struct timespec mid;
#endif

    /* Open the file. */
#ifndef OPT
    fp = fopen(DICT_FILE, "r");
    if (!fp) {
        printf("cannot open the file\n");
        exit(0);
    }
#else

#include "file.c"
#include "debug.h"
#include <fcntl.h>
#define ALIGN_FILE "align.txt"
    file_align(DICT_FILE, ALIGN_FILE, MAX_LAST_NAME_SIZE);
    int fd = open(ALIGN_FILE, O_RDONLY | O_NONBLOCK);
    var->fs = fsize(ALIGN_FILE);
#endif

    /* build the entry */
    entry *e;
    var->pHead = (entry *) malloc(sizeof(entry));
    printf("size of entry : %lu bytes\n", sizeof(entry));
    e = var->pHead;
    e->pNext = NULL;

#if defined(__GNUC__)
    __builtin___clear_cache((char *) (var->pHead), (char *) (var->pHead) + sizeof(entry));
#endif

    /* Load the input. */
#if defined(OPT)

#ifndef THREAD_NUM
#define THREAD_NUM 4
#endif
    clock_gettime(CLOCK_REALTIME, &start);

    var->map = mmap(NULL, var->fs, PROT_READ, MAP_SHARED, fd, 0);
    assert(var->map && "mmap error");

    /* allocate at beginning */
    var->num_words = (var->fs) / MAX_LAST_NAME_SIZE;
    var->entry_pool = (entry *) malloc(sizeof(entry) * (var->num_words));
    var->detail_pool = (detail*) malloc(sizeof(detail) * (var->num_words));

    assert(var->entry_pool && "entry_pool error");

    pthread_setconcurrency(THREAD_NUM + 1);

    var->tid = (pthread_t *) malloc(sizeof(pthread_t) * THREAD_NUM);
    var->thread_info = (thread_info_t **) malloc(sizeof(thread_info_t*) * THREAD_NUM);
    for (int i = 0; i < THREAD_NUM; i++)
        var->thread_info[i] = new_thread_info(var->map + MAX_LAST_NAME_SIZE * i, var->map + var->fs, i,
                                              THREAD_NUM, var->entry_pool + i, var->detail_pool + i);

    clock_gettime(CLOCK_REALTIME, &mid);
    for (int i = 0; i < THREAD_NUM; i++)
        pthread_create( &(var->tid[i]), NULL, (void *) &append, (void *) var->thread_info[i]);

    for (int i = 0; i < THREAD_NUM; i++)
        pthread_join(var->tid[i], NULL);

    var->pHead = var->thread_info[0]->pHead;
    entry *etmp = var->thread_info[0]->pLast;
    for (int i = 0; i < THREAD_NUM; i++) {
        if (i != 0) etmp->pNext = var->thread_info[i]->pHead;
        dprintf("Connect %d head string %s %p\n", i,
                var->thread_info[i]->lastName, var->thread_info[i]->ptr);

        etmp = var->thread_info[i]->pLast;
        dprintf("Connect %d tail string %s %p\n", i,
                var->thread_info[i]->pLast->lastName, var->thread_info[i]->ptr);
        dprintf("round %d\n", i);
    }
    etmp->pNext = NULL;

    clock_gettime(CLOCK_REALTIME, &end);
    var->cpu_time1 = diff_in_second(start, end);
#else /* ! OPT */
    clock_gettime(CLOCK_REALTIME, &start);
    while (fgets(line, sizeof(line), fp)) {
        while (line[i] != '\0')
            i++;
        line[i - 1] = '\0';
        i = 0;
        e = append(line, e);
        var->num_words++;
    }
    /* Because the head of the list is empty. */
    e = var->pHead;
    var->pHead = var->pHead->pNext;
    free(e);

    clock_gettime(CLOCK_REALTIME, &end);
    var->cpu_time1 = diff_in_second(start, end);
    fclose(fp);
#endif
}

#ifndef NUM_TEST
#define NUM_TEST 100
#endif

void make_test_array(struct info_t *var)
{
    char line[MAX_LAST_NAME_SIZE + 5];
    int cnt = 0;
    int i;

    srand(time(0));

    var->test_lastname = (char**) malloc(sizeof(char*) * (var->num_words));
    FILE *fp = fopen(DICT_FILE, "r");
    while (fgets(line, sizeof(line), fp)) {
        line[strlen(line) - 1] = '\0';
        var->test_lastname[cnt] = strdup(line);
        cnt++;
    }
    for(i = 0; i < (var->num_words); i++) {
        int a = rand() % (var->num_words);
        int b = rand() % (var->num_words);
        char *tem = var->test_lastname[a];
        var->test_lastname[a] = var->test_lastname[b];
        var->test_lastname[b] = tem;
    }
    fclose(fp);
}

void search(struct info_t *var)
{
    int i;
    struct timespec start, end;
#if defined(__GNUC__)
    __builtin___clear_cache((char *) var->pHead, (char *) (var->pHead) + sizeof(entry));
#endif
    /* compute the execution time */
    clock_gettime(CLOCK_REALTIME, &start);
    #pragma omp parallel for num_threads(THREAD_NUM)
    for(i = 0; i < NUM_TEST; i++)
        findName(var->test_lastname[i], var->pHead);
    clock_gettime(CLOCK_REALTIME, &end);
    var->cpu_time2 = diff_in_second(start, end);
}

void output_verified_files(struct info_t *var)
{
    FILE *veri_fp;
#if defined(OPT)
    veri_fp = fopen("veri_opt.txt", "w");
#else
    veri_fp = fopen("veri_orig.txt", "w");
#endif
    int cnt;
    entry *itr;
    for(cnt = 0, itr = var->pHead; itr != NULL; cnt++, itr = itr->pNext)
        fprintf(veri_fp, "%s\n", itr->lastName);
}

void output_execution_time(struct info_t *var)
{
    FILE *output;
#if defined(OPT)
    output = fopen("opt.txt", "a");
#else
    output = fopen("orig.txt", "a");
#endif
    fprintf(output, "append() findName() %lf %lf\n", var->cpu_time1, var->cpu_time2);
    fclose(output);

    printf("execution time of append() : %lf sec\n", var->cpu_time1);
    printf("execution time of findName() : %lf sec\n", var->cpu_time2);
}

void cleanup(struct info_t *var)
{
#ifndef OPT
    /* Don't use recursion to free linked-list.
     * It will cause stack overflow.
     */
    entry *itr, *itrNext;
    for(itr = var->pHead; itr != NULL; itr = itrNext) {
        itrNext = itr->pNext;
        free(itr);
    }
#else
    free(var->entry_pool);
    free(var->tid);
    free(var->thread_info);
    munmap(var->map, var->fs);
#endif
}
