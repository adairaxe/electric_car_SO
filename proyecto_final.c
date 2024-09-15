#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
// SHARED MEMORY
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#define MAX_WHEELS 4
#define SHM_NAME "/sv2_shared_memory"
#define NUM_LINES 4

//      ESTRUCTURAS PARA LAS LINEAS DE CARGA
typedef enum {
        AZUL_OSCURO = 0,
        NARANJA_OSCURO = 2,
        AZUL_CLARO = 1,
        NARANJA_CLARO = 3,
} LineColor;

typedef struct {
        LineColor type;
        bool is_occupied;
} ChargeLine;


//      ESTRUCTURAS PARA LA MEMORIA COMPARTIDA
typedef struct {
        bool is_regenerating;
        int regeneration_time;
        int amount_charge;
        char state_motor;
} WheelStatus;


typedef struct {
        WheelStatus wheels[MAX_WHEELS];
        int total_charge;
} SharedMemoryWheels;


//      PROTOTIPO DE FUNCIONES
void *be_wheel(void *arg);
void take_lines(int *id);
void charge_motor(int *id);
void free_lines(int *id);
void acelerating(int *id);
void *monitoring_state_drive();
void init_shared_memory_wheels();
void print_charge_after_break(int *id);
void print_charge(int *id);
char *get_state_battery();
char *get_state_motor(int *id);
void change_state_motor(int *id, char state);
void stoped();
void deleted_wheel();

// VARIABLES GLOBALES - MEMORIA COMPARTIDA
SharedMemoryWheels *shared_memory_wheels;
pthread_mutex_t shared_memory_mutex = PTHREAD_MUTEX_INITIALIZER;


// VARIABLES GLOBALES - LINEAS DE CARGA
ChargeLine lines[4] = {
        {AZUL_OSCURO, false},
        {NARANJA_OSCURO, false},
        {AZUL_CLARO, false},
        {NARANJA_CLARO, false}
};
pthread_mutex_t lines_mutex;
pthread_cond_t lines_cond;

//VARIABLES GLOBALES - ACCIONES PILOTO
char STATE_DRIVE = 'X';
int CURRENT_SPEED = 0;
pthread_mutex_t current_speed_mutex = PTHREAD_MUTEX_INITIALIZER;
int ACCELERATION;
int CRUISING_SPEED;
int DELETED_WHEEL = 3;

pthread_t monitor; 
pthread_t wheels[MAX_WHEELS];
int wheels_ids[MAX_WHEELS];

//VARIABLES GLOBALES - BATERIA
char STATE_BATTERY = 'E';

int main(int argc, char *argv[]){
        ACCELERATION = atoi(argv[1]);
        CRUISING_SPEED = atoi(argv[1]);

        int fd;

        fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0600);
        if(fd == -1){
                if(errno == EEXIST){
                        puts("La memoria compartida ya existe");
                        fd = shm_open(SHM_NAME, O_RDWR, 0600);
                        if(fd == -1){
                                perror("Error al abrir la memoria compartida");
                                exit(1);
                        }
                } else {
                        perror("Error al crear la memoria compartida");
                        exit(1);
                }

        }

        if(ftruncate(fd, sizeof(SharedMemoryWheels)) == -1){
                exit(1);
        }

        shared_memory_wheels= mmap(NULL, sizeof(SharedMemoryWheels), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        
        if(shared_memory_wheels== MAP_FAILED){  
                exit(1);
        }
        
        init_shared_memory_wheels();
        puts("Memoria compartida lista para usarse");

        int character_deleted;
        char first_character;
        do {
                puts("Ingresa A para arrancar los hilos");
                first_character = getchar();
                while ((character_deleted = getchar()) != '\n' && character_deleted != EOF);
        } while (first_character != 'A');

        STATE_DRIVE = 'A';

        pthread_create(&monitor, NULL, monitoring_state_drive, NULL);

        pthread_mutex_init(&lines_mutex, NULL);
        pthread_cond_init(&lines_cond, NULL);


        for(int i = 0; i < MAX_WHEELS; i++){
                printf("Las lineas son %d\n", lines[i]);
        }

        for(int i = 0; i < MAX_WHEELS; i++){
                wheels_ids[i] = i;
                pthread_create(&wheels[i], NULL, be_wheel, &wheels_ids[i]);
        }

        for(int i = 0; i < MAX_WHEELS; i++){
             pthread_join(wheels[i], NULL);
        }

        pthread_join(monitor, NULL);

        pthread_mutex_destroy(&lines_mutex);
        pthread_cond_destroy(&lines_cond);
        pthread_mutex_destroy(&current_speed_mutex);
      
        if(munmap(shared_memory_wheels, sizeof(SharedMemoryWheels)) == -1){
                perror("munmap");
                exit(1);
        }

        if(close(fd) == -1){
                perror("Close the fd");
                exit(1);
        }

        if(shm_unlink(SHM_NAME) == -1){
                perror("shm_unlink");
                exit(1);
        }

        if(pthread_mutex_destroy(&shared_memory_mutex) != 0){
                puts("Error al destruir el semaforo");
        }

        return 0;
}

void *be_wheel(void *arg){
        while(shared_memory_wheels->total_charge > 0){
                if(STATE_DRIVE == 'A'){
                        change_state_motor(arg, 'A');
                        STATE_BATTERY = 'D';
                        acelerating(arg);
                        usleep(100000);
                        print_charge(arg);

                } else if (STATE_DRIVE == 'F'){
                        STATE_BATTERY = 'C';
                        change_state_motor(arg, 'R');
                        for(int i = 0; i < 10; i++){
                                take_lines(arg);
                                charge_motor(arg);
                                free_lines(arg);
                                usleep(10000);
                        }
                        print_charge_after_break(arg);
                        STATE_DRIVE = 'D';
                        STATE_BATTERY = 'D';

                } else if (STATE_DRIVE == 'X'){
                        break;
                } else if (STATE_DRIVE == 'D'){
                        change_state_motor(arg, 'S');
                        stoped();
                        usleep(100000);
                        print_charge(arg);
                }
        }
        pthread_cancel(monitor);
        return NULL;
}

void acelerating(int *id){
        pthread_mutex_lock(&current_speed_mutex); 
        if(CURRENT_SPEED < CRUISING_SPEED){
                CURRENT_SPEED += ACCELERATION; 
        }
        pthread_mutex_unlock(&current_speed_mutex);
        shared_memory_wheels->wheels[*id].is_regenerating = false;
        pthread_mutex_lock(&shared_memory_mutex);
        shared_memory_wheels->total_charge -= 0.1 * CURRENT_SPEED / CRUISING_SPEED;
        pthread_mutex_unlock(&shared_memory_mutex);
}

void stoped(){
        puts("Estas parado");
        pthread_mutex_lock(&shared_memory_mutex);
        shared_memory_wheels->total_charge -= 1;
        pthread_mutex_unlock(&shared_memory_mutex);
}

void damage_wheel(int id){
        if(DELETED_WHEEL == 0){
                puts("Ya no tienes llantas");
                pthread_cancel(wheels[id]);
                pthread_cancel(monitor);
        }
        pthread_mutex_lock(&lines_mutex);
        lines[id].is_occupied = false;
        lines[(id+1) % MAX_WHEELS].is_occupied = false;
        pthread_mutex_unlock(&lines_mutex);
        pthread_cancel(wheels[id]);
        DELETED_WHEEL -= 1;
        for(int i = 0; i < 3; i++){
                printf("La rueda %d ha reventado\n", id);
                usleep(10000);
        }
}

void take_lines(int *id){
        pthread_mutex_lock(&lines_mutex);         
        while(lines[*id].is_occupied || lines[(*id+1) % MAX_WHEELS].is_occupied){
               pthread_cond_wait(&lines_cond, &lines_mutex);
        }
        lines[*id].is_occupied = true;
        lines[(*id+1) % MAX_WHEELS].is_occupied = true;
        pthread_mutex_unlock(&lines_mutex);
}

void charge_motor(int *id){
        shared_memory_wheels->wheels[*id].is_regenerating = true;
        shared_memory_wheels->wheels[*id].regeneration_time += 1;
        pthread_mutex_lock(&current_speed_mutex);
        if(CURRENT_SPEED > 0){
                CURRENT_SPEED -= 2 * ACCELERATION;
        }
        pthread_mutex_unlock(&current_speed_mutex);
        printf("Motor de la rueda %d está %s\n", *id, get_state_motor(id));
        shared_memory_wheels->wheels[*id].amount_charge += 0.05* shared_memory_wheels->wheels[*id].regeneration_time /5 * CURRENT_SPEED / CRUISING_SPEED;

}

void change_state_motor(int *id, char state){
        shared_memory_wheels->wheels[*id].state_motor = state;
}

void free_lines(int *id){
        pthread_mutex_lock(&lines_mutex);
        lines[*id].is_occupied = false;  
        lines[(*id+1) % MAX_WHEELS].is_occupied = false;
        pthread_cond_broadcast(&lines_cond);
        pthread_mutex_unlock(&lines_mutex);
}

void *monitoring_state_drive(){
        char instruction_driver[32];
        while(fgets(instruction_driver, sizeof(instruction_driver), stdin)){
                instruction_driver[strcspn(instruction_driver, "\n")] = 0;
                if(strcmp(instruction_driver, "A") == 0){
                        STATE_DRIVE = 'A';
                } else if (strcmp(instruction_driver, "F") == 0){
                        STATE_DRIVE = 'F';
                } else if (strcmp(instruction_driver, "X") == 0){
                        STATE_DRIVE = 'X';
                        break;
                } else if (strcmp(instruction_driver, "M") == 0){
                        damage_wheel(DELETED_WHEEL);
                }
        }
        return NULL;
}

void print_charge_after_break(int *id){
        pthread_mutex_lock(&shared_memory_mutex);
        shared_memory_wheels->total_charge += shared_memory_wheels->wheels[*id].amount_charge;
        shared_memory_wheels->wheels[*id].amount_charge = 0;
        printf("La regeneración ha terminado, el porcentaje total de carga es: %d \n" , shared_memory_wheels->total_charge);
        pthread_mutex_unlock(&shared_memory_mutex);
}


void print_charge(int *id){
        pthread_mutex_lock(&shared_memory_mutex);
        printf("El motor de la rueda %d está %s.La bateria está %s y su porcentaje es: %d \n" ,*id, get_state_motor(id) ,get_state_battery(), shared_memory_wheels->total_charge);
        pthread_mutex_unlock(&shared_memory_mutex);
}

char *get_state_battery(){
        switch(STATE_BATTERY){
                case 'D':
                        return "DESCARGANDO";
                case 'C':
                        return "CARGANDO";
        }
}

char *get_state_motor(int *id){
        switch(shared_memory_wheels->wheels[*id].state_motor){
                case 'S':
                        return "SIN_EFECTO";
                case 'A':
                        return "ACELERANDO";
                case 'R':
                        return "REGENERANDO";
        }
}

void init_shared_memory_wheels(){
        for (int i = 0; i < MAX_WHEELS; i++){
                shared_memory_wheels->wheels[i].is_regenerating = false;
                shared_memory_wheels->wheels[i].regeneration_time = 0;
                shared_memory_wheels->wheels[i].amount_charge = 0;
                shared_memory_wheels->wheels[i].state_motor = 'S';
        }
        shared_memory_wheels->total_charge = 100;
}
