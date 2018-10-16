#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <math.h>
#include "stack.h"
#include "math_function.h"

#define MAX_SIZE 1e-16
#define FALSE 0
#define TRUE 1
#define PROCESSAR 1
#define LIBERAR 2
#define DIVIDIR 3
#define PROCESSADO 4

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

double coordinator(int process, double start_at, double end_at) {
    double result = 0;
    MPI_Status status;

    stack* bag;
    bag = new_stack();
    double sync[] = {start_at, end_at}; 
    push(sync, bag);

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
        if (status.MPI_TAG == DIVIDIR) {
            push(sync, bag);
            MPI_Recv(sync, 2, MPI_DOUBLE, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
            push(sync, bag);
        } else {
            result = result + sync[0];
        }
        feed(bag, executors, total_executors);
    }
    release(executors, total_executors);
    return result;
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
        double left = sync[0];
        double right = sync[1];
        double lrarea = (F(left) + F(right)) * (right - left)/2;
        double mid, fmid, larea, rarea;

        mid = (left + right)/2;
        fmid = F(mid);
        larea = (F(left) + fmid) * (mid - left)/2;
        rarea = (fmid + F(right)) * (right - mid)/2;
        double total_area = larea + rarea;

        double size = (total_area) - lrarea;
        if (size < 0){
            size *= -1;
        }
        if (size > MAX_SIZE) {
            sync[0] = left; 
            sync[1] = mid;
            //printf("%d - DIVIDI 1/2\n", me);
            end_calc = MPI_Wtime();
            calc += end_calc - start_calc;
            start_con = MPI_Wtime();
            MPI_Send(sync, 2, MPI_DOUBLE, 0, DIVIDIR, MPI_COMM_WORLD);
            sync[0] = mid; 
            sync[1] = right;
            //printf("%d - DIVIDI 2/2\n", me);
            MPI_Send(sync, 2, MPI_DOUBLE, 0, DIVIDIR, MPI_COMM_WORLD);
            end_con = MPI_Wtime();
            con += end_con - start_con;
        }
        else {
            sync[0] = total_area; 
            sync[1] = 0;
            end_calc = MPI_Wtime();
            calc += end_calc - start_calc;
            start_con = MPI_Wtime();
            MPI_Send(sync, 2, MPI_DOUBLE, 0, PROCESSADO, MPI_COMM_WORLD);
            end_con = MPI_Wtime();
            con += end_con - start_con;
        }
        //printf("%d - ESPERANDO\n", me);
        start_con = MPI_Wtime();
        MPI_Recv(sync, 2, MPI_DOUBLE, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        end_con = MPI_Wtime();
        con += end_con - start_con;
        //printf("%d - RECEBI %d\n", me, status.MPI_TAG);
    }
    printf("Time-network (%d): %f\n", me, con);
    printf("Time-calculator (%d): %f\n", me, calc);
    //printf("%d - FUI LIBERADO\n", me);
}

int main(int argc, char **argv ) {
    double start = MPI_Wtime();
    double end;
    int i, myid, numprocs;
    double area;
    double start_at = atof(argv[1]);
    double end_at = atof(argv[2]);
    function_id = atoi(argv[3]);

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    
    double start_coordinator, end_coordinator;
    double start_executor, end_executor;
    if (myid == 0) {
        //printf("LIDER %d\n", myid);
        start_coordinator = MPI_Wtime();
        area = coordinator(numprocs, start_at, end_at);
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
        //printf("Area=%0.2f\n", area);
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
