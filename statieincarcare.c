#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX_PUMPS 3
#define MAX_QUEUE_SIZE 6

typedef struct {
    int id;
    int requested_current;
} Client;

typedef struct {
    int battery_capacity;
    int current_battery_level;
    sem_t pump_sem;
    sem_t payment_sem;
    sem_t charging_sem;  // Semafor pentru a restricționa încărcarea de la panouri solare în timpul alimentării unui client
} Pump;

Pump pumps[MAX_PUMPS];

sem_t station_sem;
sem_t queue_sem;
sem_t queue_mutex;
int current_queue_size = 0;
int current_client_id = 1;

void *station_action(void *arg);

void *client_action(void *arg) {
    Client *client = (Client *)arg;

    // Generare aleatoare a cantității de curent cerute de client (între 0 și 10)
    client->requested_current = rand() % 11;

    printf("Client %d arrived. Requested current: %dA.\n", client->id, client->requested_current);

    sem_wait(&queue_mutex);

    if (current_queue_size < MAX_QUEUE_SIZE) {
        current_queue_size++;
        sem_post(&queue_mutex);

        Pump *selected_pump = &pumps[client->id % MAX_PUMPS];

        sem_wait(&selected_pump->pump_sem);

        if (selected_pump->current_battery_level >= 0 && selected_pump->current_battery_level <= 20) {
            printf("Client %d at pump %d charging from the grid. Battery level: %d/%dA.\n", client->id, client->id % MAX_PUMPS, selected_pump->current_battery_level, selected_pump->battery_capacity);
            sleep(1);  // Simulare încărcare din rețea

            // Încarcă bateria de la panourile solare simultan doar dacă pompa este liberă
            sem_wait(&selected_pump->charging_sem);
            if (selected_pump->current_battery_level < 20) {
                printf("Battery charging from solar panels.\n");
                if (selected_pump->current_battery_level > 90) {
            int remaining_capacity = selected_pump->battery_capacity - selected_pump->current_battery_level;
            selected_pump->current_battery_level += selected_pump->battery_capacity - selected_pump->current_battery_level;
            printf("Charged only with remaining capacity: %dA. Total battery level: %d/%dA.\n", remaining_capacity, selected_pump->current_battery_level, selected_pump->battery_capacity);
        } else {
            selected_pump->current_battery_level += 10;  // Exemplu de încărcare de la panouri solare
            printf("Client %d paid at pump %d with battery level: %d/%dA.\n", client->id, client->id % MAX_PUMPS, selected_pump->current_battery_level, selected_pump->battery_capacity);
        }
                
                if (selected_pump->current_battery_level > selected_pump->battery_capacity) {
                    selected_pump->current_battery_level = selected_pump->battery_capacity;
                }
            } else {
                printf("Battery charging from solar panels restricted due to low battery level.\n");
            }
            sem_post(&selected_pump->charging_sem);
        } else {
            printf("Client %d at pump %d with battery level: %d/%dA.\n", client->id, client->id % MAX_PUMPS, selected_pump->current_battery_level, selected_pump->battery_capacity);
            sleep(1);  // Simulează încărcare
        }

        // Scade cantitatea de curent cerută din nivelul curent al bateriei
        selected_pump->current_battery_level -= client->requested_current;

        // Verificare pentru a nu depăși 100 la încărcarea bateriei
        

        sem_post(&selected_pump->pump_sem);

        sem_post(&station_sem);

        sem_wait(&queue_sem);
        printf("Client %d leaves.\n", client->id);
        current_queue_size--;
        sem_post(&queue_sem);
    } else {
        sem_post(&queue_mutex);
        printf("Queue is full. Client %d leaves.\n", client->id);
    }
    free(client);  // Eliberăm memoria alocată pentru client
    pthread_exit(NULL);
}

void initialize() {
    for (int i = 0; i < MAX_PUMPS; i++) {
        pumps[i].battery_capacity = 100; // Exemplu de capacitate a bateriei
        pumps[i].current_battery_level = 100; // Exemplu de nivel curent al bateriei
        sem_init(&pumps[i].pump_sem, 0, 1);
        sem_init(&pumps[i].payment_sem, 0, 1);
        sem_init(&pumps[i].charging_sem, 0, 1);
    }

    sem_init(&station_sem, 0, 1);
    sem_init(&queue_sem, 0, 1);
    sem_init(&queue_mutex, 0, 1);

    pthread_t pumps_threads[MAX_PUMPS];
    for (int i = 0; i < MAX_PUMPS; i++) {
        pthread_create(&pumps_threads[i], NULL, station_action, (void *)&pumps[i]);
    }
}

void *station_action(void *arg) {
    Pump *current_pump = (Pump *)arg;

    while (1) {
        sem_post(&current_pump->pump_sem); // Pompa este disponibilă
        sleep(10); // Verifică la fiecare 10 secunde
    }
}

void wait_for_user_input() {
    char input;
    while (1) {
        scanf(" %c", &input);
        if (input == 'c') {
            Client *new_client = malloc(sizeof(Client));
            new_client->id = current_client_id++;
            pthread_t client_thread;
            pthread_create(&client_thread, NULL, client_action, (void *)new_client);
        } else if (input == 'b') {
            printf("Battery levels:\n");
            for (int i = 0; i < MAX_PUMPS; i++) {
                printf("Pump %d: %d/%dA\n", i, pumps[i].current_battery_level, pumps[i].battery_capacity);
            }
        } else {
            break;
        }
    }
}

int main() {
    initialize();

    pthread_t station_thread;
    pthread_create(&station_thread, NULL, station_action, (void *)&pumps[0]);

    wait_for_user_input();

    return 0;
}