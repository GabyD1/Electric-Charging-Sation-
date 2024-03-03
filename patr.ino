
#include <Arduino.h>
#include <FreeRTOS.h>
#include <semphr.h>


#define MAX_PUMPS 3
#define MAX_QUEUE_SIZE 6

struct Client {
    int id;
    int requested_current;
};

struct Pump {
    int battery_capacity;
    int current_battery_level;
    SemaphoreHandle_t pump_sem;
    SemaphoreHandle_t payment_sem;
    SemaphoreHandle_t charging_sem;
};

struct Light {
    int pin;
    bool is_on;
};

Pump pumps[MAX_PUMPS];
SemaphoreHandle_t station_sem;
SemaphoreHandle_t queue_sem;
SemaphoreHandle_t queue_mutex;
int current_queue_size = 0;
int current_client_id = 1;

Light lights[MAX_PUMPS] = {
    {2, false},  // Pin 2 for Light 0
    {3, false},  // Pin 3 for Light 1
    {4, false}   // Pin 4 for Light 2
};

Client* new_client = nullptr;  // Variabila globala

void initialize();
void* station_action(void* pvParameters);
void* client_action(void* pvParameters);
void wait_for_user_input();

void setup() {
    initialize();

    for (int i = 0; i < MAX_PUMPS; i++) {
        pinMode(lights[i].pin, OUTPUT);
    }

    xTaskCreate(station_action, "Station", 10000, NULL, 1, NULL, 0);
    xTaskCreate(client_action, "Client", 10000, NULL, 1, NULL, 1);
}

void loop() {
    wait_for_user_input();
    vTaskDelay(100);
}

void initialize() {
    for (int i = 0; i < MAX_PUMPS; i++) {
        pumps[i].battery_capacity = 100;
        pumps[i].current_battery_level = 100;
        pumps[i].pump_sem = xSemaphoreCreateMutex();
        pumps[i].payment_sem = xSemaphoreCreateMutex();
        pumps[i].charging_sem = xSemaphoreCreateMutex();
    }

    station_sem = xSemaphoreCreateMutex();
    queue_sem = xSemaphoreCreateMutex();
    queue_mutex = xSemaphoreCreateMutex();
}

void* station_action(void* pvParameters) {
    Pump* current_pump = &pumps[0];
    while (1) {
        xSemaphoreGive(current_pump->pump_sem);

        for (int i = 0; i < MAX_PUMPS; i++) {
            if (xSemaphoreTake(pumps[i].pump_sem, 0) == pdTRUE) {
                lights[i].is_on = true;
                digitalWrite(lights[i].pin, HIGH);
            } else {
                lights[i].is_on = false;
                digitalWrite(lights[i].pin, LOW);
            }
        }

        vTaskDelay(10000); // Verifică la fiecare 10 secunde
    }
}

void* client_action(void* pvParameters) {
    Client* client = (Client*)pvParameters;
    client->requested_current = random(0, 11);

    Serial.print("Client ");
    Serial.print(client->id);
    Serial.print(" arrived. Requested current: ");
    Serial.print(client->requested_current);
    Serial.println("A.");

    xSemaphoreTake(queue_mutex, portMAX_DELAY);

    if (current_queue_size < MAX_QUEUE_SIZE) {
        current_queue_size++;
        xSemaphoreGive(queue_mutex);

        Pump* selected_pump = &pumps[client->id % MAX_PUMPS];

        xSemaphoreTake(selected_pump->pump_sem, portMAX_DELAY);

        if (selected_pump->current_battery_level >= 0 && selected_pump->current_battery_level <= 20) {
            Serial.print("Client ");
            Serial.print(client->id);
            Serial.print(" at pump ");
            Serial.print(client->id % MAX_PUMPS);
            Serial.print(" charging from the grid. Battery level: ");
            Serial.print(selected_pump->current_battery_level);
            Serial.print("/");
            Serial.print(selected_pump->battery_capacity);
            Serial.println("A.");
            delay(1000);

            xSemaphoreTake(selected_pump->charging_sem, portMAX_DELAY);
            if (selected_pump->current_battery_level > 90) {
                int remaining_capacity = selected_pump->battery_capacity - selected_pump->current_battery_level;
                selected_pump->current_battery_level += remaining_capacity;
                Serial.print("Charged only with remaining capacity: ");
                Serial.print(remaining_capacity);
                Serial.print("A. Total battery level: ");
                Serial.print(selected_pump->current_battery_level);
                Serial.print("/");
                Serial.print(selected_pump->battery_capacity);
                Serial.println("A.");
            } else {
                selected_pump->current_battery_level += 10;
                Serial.print("Client ");
                Serial.print(client->id);
                Serial.print(" paid at pump ");
                Serial.print(client->id % MAX_PUMPS);
                Serial.print(" with battery level: ");
                Serial.print(selected_pump->current_battery_level);
                Serial.print("/");
                Serial.print(selected_pump->battery_capacity);
                Serial.println("A.");
            }

            xSemaphoreGive(selected_pump->charging_sem);
        } else {
            Serial.print("Client ");
            Serial.print(client->id);
            Serial.print(" at pump ");
            Serial.print(client->id % MAX_PUMPS);
            Serial.print(" with battery level: ");
            Serial.print(selected_pump->current_battery_level);
            Serial.print("/");
            Serial.print(selected_pump->battery_capacity);
            Serial.println("A.");
            delay(1000);
        }

        selected_pump->current_battery_level -= client->requested_current;

        if (selected_pump->current_battery_level > 90) {
            int remaining_capacity = selected_pump->battery_capacity - selected_pump->current_battery_level;
            selected_pump->current_battery_level += remaining_capacity;
            Serial.print("Charged only with remaining capacity: ");
            Serial.print(remaining_capacity);
            Serial.print("A. Total battery level: ");
            Serial.print(selected_pump->current_battery_level);
            Serial.print("/");
            Serial.print(selected_pump->battery_capacity);
            Serial.println("A.");
        } else {
            Serial.print("Client ");
            Serial.print(client->id);
            Serial.print(" paid at pump ");
            Serial.print(client->id % MAX_PUMPS);
            Serial.print(" with battery level: ");
            Serial.print(selected_pump->current_battery_level);
            Serial.print("/");
            Serial.print(selected_pump->battery_capacity);
            Serial.println("A.");
        }

        xSemaphoreGive(selected_pump->pump_sem);

        xSemaphoreGive(station_sem);

        xSemaphoreTake(queue_sem, portMAX_DELAY);
        Serial.print("Client ");
        Serial.print(client->id);
        Serial.println(" leaves.");
        current_queue_size--;
        xSemaphoreGive(queue_sem);
    } else {
        xSemaphoreGive(queue_mutex);
        Serial.print("Queue is full. Client ");
        Serial.print(client->id);
        Serial.println(" leaves.");
    }
    delete client;
    vTaskDelete(NULL);
}

void wait_for_user_input() {
    if (Serial.available() > 0) {
        char input = Serial.read();
        if (input == 'c') {
            new_client = new Client;
            new_client->id = current_client_id++;
            xTaskCreate(client_action, "Client", 10000, (void*)new_client, 1, NULL);
        } else if (input == 'b') {
            Serial.println("Battery levels:");
            for (int i = 0; i < MAX_PUMPS; i++) {
                Serial.print("Pump ");
                Serial.print(i);
                Serial.print(": ");
                Serial.print(pumps[i].current_battery_level);
                Serial.print("/");
                Serial.print(pumps[i].battery_capacity);
                Serial.println("A");
            }
        }
    }
}
