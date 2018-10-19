#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <math.h>
#include "stack.h"
#include "math_function.h"

#define MAX_SIZE 1e-12

#define FALSE 0
#define TRUE 1
#define PROCESSAR 1
#define LIBERAR 2
#define PROCESSADO 3

int function_id;

double F(double arg) {
    math_func_def math_func = get_math_function(function_id);
    return math_func(arg);
}

int is_done(stack* bag, int* executors, int total_executors) {
    if (!is_empty(bag)) {
        return FALSE;
    }
    for (int i=0; i<total_executors; i++){
        if (executors[i] == TRUE){
            return FALSE;
        }
    }
    return TRUE;
}

void feed(stack* bag, int* executors, int total_executors) {
    for (int i=0; i<total_executors; i++){
        if (!is_empty(bag) && executors[i] == FALSE) {
            //printf("LIDER - ENVIANDO PARA %d\n", i+1);
            MPI_Send(pop(bag), 2, MPI_DOUBLE, i+1, PROCESSAR, MPI_COMM_WORLD);
            executors[i] = TRUE;
        }
    }
}

void release(int* executors, int total_executors) {
    for (int i=0; i<total_executors; i++){
        //printf("LIDER - LIBERANDO %d\n", i+1);
        MPI_Send(NULL, 0, MPI_DOUBLE, i+1, LIBERAR, MPI_COMM_WORLD);
    }
}

void split(stack* bag, double start_at, double end_at, double interval) {
    double sync[2];
    interval = (end_at - start_at) / interval;
    for (double i=start_at; i<end_at; i=i+interval){
        sync[0] = i;
        sync[1] = i+interval;
        if (sync[1] > end_at) {
            sync[1] = end_at;
        }
        push(sync, bag);
        //printf("X - %f %f\n", sync[0], sync[1]);
    }
}

double coordinator(int process, double start_at, double end_at, double interval) {
    double result = 0;
    MPI_Status status;

    stack* bag;
    bag = new_stack();
    double sync[2]; 
    split(bag, start_at, end_at, interval);

    int total_executors = process - 1;
    int* executors = (int*) malloc(sizeof(int)*(total_executors));
    for (int i=0; i<total_executors; i++){
        executors[i] = FALSE;
    }

    feed(bag, executors, total_executors);
    while (is_done(bag, executors, total_executors) == FALSE) {
        //printf("LIDER - ESPERANDO RESPOSTAS...\n");
        MPI_Recv(sync, 2, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        executors[status.MPI_SOURCE - 1] = FALSE;
        //printf("LIDER - RECEBI DE %d %d\n", status.MPI_SOURCE, status.MPI_TAG);
        result = result + sync[0];
        feed(bag, executors, total_executors);
    }
    release(executors, total_executors);
    return result;
}

double calc_area(double left_size, double right_size) {
    double base = (right_size - left_size);
    double mid = (right_size + left_size) / 2.0;
    double fmid = F(mid);
    double larea = base * ( (F(left_size) + fmid) / 2.0 );
    double rarea = base * ( (fmid + F(right_size)) / 2.0 );
    
    double total_area = larea;
    double size = rarea - larea;
    if (size < 0) { size *= -1; }
    if (size > MAX_SIZE) {
        larea = calc_area(left_size, mid);
        rarea = calc_area(mid, right_size);
        total_area = larea + rarea;
    }
    return total_area;
}

void executor(int me) {
    double start_con, end_con, con = 0.0;
    double start_calc, end_calc, calc = 0.0;
    MPI_Status status;
    double sync[2];
    
    //printf("%d - ESPERANDO\n", me);
    start_con = MPI_Wtime();
    MPI_Recv(sync, 2, MPI_DOUBLE, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    end_con = MPI_Wtime();
    con += end_con - start_con;
    //printf("%d - RECEBI %d\n", me, status.MPI_TAG);
    while (status.MPI_TAG == PROCESSAR) {
        start_calc = MPI_Wtime();
        sync[0] = calc_area(sync[0], sync[1]);
        sync[1] = 0;
        end_calc = MPI_Wtime();
        calc += end_calc - start_calc;
        start_con = MPI_Wtime();
        MPI_Send(sync, 2, MPI_DOUBLE, 0, PROCESSADO, MPI_COMM_WORLD);
        //printf("%d - ESPERANDO\n", me);
        MPI_Recv(sync, 2, MPI_DOUBLE, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        //printf("%d - RECEBI %d\n", me, status.MPI_TAG);
        end_con = MPI_Wtime();
        con += end_con - start_con;
    }
    //printf("%d - FUI LIBERADO\n", me);
    printf("Time-network (%d): %f\n", me, con);
    printf("Time-calculator (%d): %f\n", me, calc);
}

int main(int argc, char **argv ) {
    double start = MPI_Wtime();
    double end;
    int i, myid, numprocs;
    double area;
    double start_at = atof(argv[1]);
    double end_at = atof(argv[2]);
    double interval = atof(argv[3]);
    function_id = atoi(argv[4]);    
    
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    
    double start_coordinator, end_coordinator;
    double start_executor, end_executor;
    if (myid == 0) {
        //printf("LIDER %d\n", myid);
        start_coordinator = MPI_Wtime();
        area = coordinator(numprocs, start_at, end_at, interval);
        end_coordinator = MPI_Wtime();
        printf("Time-coordinator (%d): %f\n", myid, end_coordinator - start_coordinator);
    } else {
        //printf("EXECUTOR %d\n", myid);
        start_executor = MPI_Wtime();
        executor(myid);
        end_executor = MPI_Wtime();
        printf("Time-executor (%d): %f\n", myid, end_executor - start_executor);
    }

    end = MPI_Wtime();
    if(myid == 0) {
        printf("Result: %0.20f\n", area);
        printf("Time-start-program (%d): %f\n", myid, start_coordinator - start);
        printf("Time-diff-end-end (%d): %f\n", myid, end - end_coordinator);
    } else {
        printf("Time-start-program (%d): %f\n", myid, start_executor - start);
        printf("Time-diff-end-end (%d): %f\n", myid, end - end_executor);
    }
    printf("Time (%d): %f\n", myid, end - start);
    
    MPI_Finalize();
    return 0;
}
