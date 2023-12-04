/**
 * @file carControlSystem.ino
 * @author Julie Anne (julie.cantillep@studerande.movant.se)
 * @brief A program that demonstrates a car control system using the FreeRTOS
 * framework like motor, ventilation, and fuel subsystems.
 * @version 0.1
 * @date 2023-12-04
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Define queues, queue set, and semaphore for inter-task communication
QueueHandle_t motorQueue, ventilationQueue, fuelQueue, speedQueue, rpmQueue;
QueueSetHandle_t queueSet;
SemaphoreHandle_t dashboardSemaphore;

// Structure to hold car status information
struct CarStatus
{
  uint8_t speed;
  uint32_t rpm;
  bool ventilationStatus;
  float fuelLevel;

};

// default global initialization of CarStatus struct
CarStatus carStatus = {0, 0, false, 0.0f };

// Time to wait between task executions
TickType_t xTicksToWait = pdMS_TO_TICKS(1000);

// Function prototypes
void motorControlTask(void *pvParameters);
void ventilationControlTask(void *pvParameters);
void fuelControlTask(void *pvParameters);
void dashboardTask(void *pvParameters);
void sendMessageToSubsystem(const char *selfCheckMsg, QueueHandle_t subsystemQueue, const char *checkMsg);
bool performSelfCheck();
bool ventilationCheck();
bool performFuelCheck();
float fuelLevel();

// Setup function
void setup()
{
  // Initialize serial communication
  Serial.begin(921600);
  Serial.println(F("******* Program Start ********"));

  // Create queues for inter-task communication
  motorQueue = xQueueCreate(5, sizeof(char *));
  ventilationQueue = xQueueCreate(5, sizeof(char *));
  fuelQueue = xQueueCreate(5, sizeof(char *));
  speedQueue = xQueueCreate(1, sizeof(int *));
  rpmQueue = xQueueCreate(1, sizeof(int *));

  // Create a queue set to manage multiple queues
  queueSet = xQueueCreateSet(5 + 5 + 5 + 1 + 1 + 1);
  xQueueAddToSet(motorQueue, queueSet);
  xQueueAddToSet(ventilationQueue, queueSet);
  xQueueAddToSet(fuelQueue, queueSet);
  xQueueAddToSet(speedQueue, queueSet);
  xQueueAddToSet(rpmQueue, queueSet);

  // Create a semaphore to protect shared resources
  dashboardSemaphore = xSemaphoreCreateMutex();

  // Create tasks for motor control, ventilation control, fuel control, and dashboard
  xTaskCreate(motorControlTask, "Master Motor Control System", 128, NULL, 1, NULL);
  xTaskCreate(ventilationControlTask, "Ventilation Control System", 128, NULL, 1, NULL);
  xTaskCreate(fuelControlTask, "Fuel Control System", 128, NULL, 1, NULL);
  xTaskCreate(dashboardTask, "Dashboard", 128, NULL, 1, NULL);

  // Start FreeRTOS scheduler
  vTaskStartScheduler();
}

// Motor control task
void motorControlTask(void *pvParameters)
{
  const char *checkMsg = "Checking motor/vent/fuel";
  char *statusMsg, *statusMsg2;
  BaseType_t qStatus, qStatus2, qStatus3, qStatus4;

  while (1)
  {
    // Simulate self-check
    if (performSelfCheck())
    {
      statusMsg = "M.G is ok, speed xx and rpm is yyyy";
      statusMsg2 = "Motor is OK";
      carStatus.speed = 90;
      carStatus.rpm = 2500;
    }
    else
    {
      statusMsg = "x01: Error: M.||Gb.";
      statusMsg2 = "Motor is not OK";
    }

    // Acquire semaphore before accessing motorQueue
    qStatus = xQueueSend(motorQueue, &statusMsg, 0);
    int speedValue = carStatus.speed;

    if (qStatus == pdPASS)
    {
      Serial.println(F("Motor Message Sent"));
      vTaskDelay(xTicksToWait); // Every 1 second

      // Send messages to ventilation and fuel
      sendMessageToSubsystem(checkMsg, ventilationQueue, "Checking vent.");
      sendMessageToSubsystem(checkMsg, fuelQueue, "Checking Fuel");

      // Send speed value to speedQueue
      qStatus2 = xQueueSend(speedQueue, &carStatus.speed, 0);
      if (qStatus2 == pdPASS)
      {
        Serial.println(F("Motor Speed Message Sent"));
        vTaskDelay(xTicksToWait);
      }

      // Send RPM value to rpmQueue
      qStatus3 = xQueueSend(rpmQueue, &carStatus.rpm, 0);
      {
        if (qStatus3 == pdPASS)
        {
          Serial.println(F("RPM Message Sent"));
          vTaskDelay(xTicksToWait);
        }
      }

      // Send additional message to motorQueue
      qStatus4 = xQueueSend(motorQueue, &statusMsg2, 0);
      if (qStatus4 == pdPASS)
      {
        Serial.println(F("Motor Message 2 Sent"));
        vTaskDelay(xTicksToWait);
      }
    }
  }
}

// Ventilation control task
void ventilationControlTask(void *pvParameters)
{
  char *statusMsg, *statusMsg2;
  BaseType_t qStatus, qStatus2;

  while (1)
  {
    // Check ventilation status
    if (ventilationCheck())
    {
      statusMsg = "Vent. is ok";
      statusMsg2 = "Y*Y*";
    }
    else
    {
      statusMsg = "x02:Error: Vent";
      statusMsg2 = "N*N*";
    }

    // Send messages to ventilationQueue
    qStatus = xQueueSend(ventilationQueue, &statusMsg, portMAX_DELAY);

    if (qStatus == pdPASS)
    {
      Serial.println("Ventilation Message Sent");
      vTaskDelay(xTicksToWait);

      // Only attempt to send the second message if the first one was successful
      qStatus2 = xQueueSend(ventilationQueue, &statusMsg2, portMAX_DELAY);

      if (qStatus2 == pdPASS)
      {
        Serial.println("Ventilation 2 Message Sent");
        vTaskDelay(xTicksToWait);
      }
    }
  }
}

// Fuel control task
void fuelControlTask(void *pvParameters)
{
  char *statusMsg, *statusMsg2;
  BaseType_t qStatus, qStatus2;

  while (1)
  {
    // Simulate fuel check logic
    if (performFuelCheck() && fuelLevel() < 10.00)
    {
      statusMsg = "0x3: low fuel";
      statusMsg2 = "U$U$";
    }
    else if (performFuelCheck() && fuelLevel() > 10.00)
    {
      statusMsg = "0x4 good fuel";
      statusMsg2 = "h$h$";
    }
    else
    {
      statusMsg = "Fuel.is.xx.yy%";
    }

    // Send messages to fuelQueue
    qStatus = xQueueSend(fuelQueue, &statusMsg, portMAX_DELAY);

    if (qStatus == pdPASS)
    {
      Serial.println("Fuel Message Sent");
      vTaskDelay(xTicksToWait);
    }

    // Send additional message to fuelQueue
    qStatus2 = xQueueSend(fuelQueue, &statusMsg2, portMAX_DELAY);

    if (qStatus2 == pdPASS)
    {
      Serial.println("Fuel Message 2 Sent");
      vTaskDelay(xTicksToWait);
    }
  }
}

// Dashboard task
void dashboardTask(void *pvParameters)
{
  char *dashboardMsg;
  int speedValue, rpmValue;
  BaseType_t qStatus, qStatus2;

  while (1)
  {
    // Use semaphore to protect access to shared resources
    xSemaphoreTake(dashboardSemaphore, portMAX_DELAY);

    // Read and print messages from all subsystems
    char *msg;
    QueueHandle_t activeQueue, activeQueue2;

    while (1)
    {
      // Use xQueueSelectFromSet with timeout to avoid busy waiting
      activeQueue = (QueueHandle_t)xQueueSelectFromSet(queueSet, pdMS_TO_TICKS(100));

      if (activeQueue != NULL)
      {

        qStatus = xQueueReceive(activeQueue, &msg, 0);

        if (qStatus == pdPASS)
        {
          vTaskDelay(pdMS_TO_TICKS(100));
          Serial.println(F("Received: "));
          Serial.println(msg);
        }

        // Check if the received message is from the speedQueue
        if (activeQueue == speedQueue)
        {
          // Convert the message to an integer (assuming it's an integer)
          speedValue = (int)msg;
          Serial.print(F("Speed: "));
          Serial.println(speedValue);
        }

        // Check if the received message is from the rpmQueue
        if (activeQueue == rpmQueue)
        {
          // Convert the message to an integer (assuming it's an integer)
          rpmValue = (int)msg;
          Serial.print(F("RPM: "));
          Serial.println(rpmValue);
        }
      }
      else
      {
        break; // No more messages in the set
      }
    }

    // Release semaphore
    xSemaphoreGive(dashboardSemaphore);
  }
}

// Function to send a message to a subsystem and wait for a response
void sendMessageToSubsystem(const char *selfCheckMsg, QueueHandle_t subsystemQueue, const char *checkMsg)
{
  char *statusMsg;
  BaseType_t qStatus = xQueueSend(subsystemQueue, &checkMsg, 0);

  if (qStatus == pdPASS)
  {
    // Simulate subsystem response
    qStatus = xQueueReceive(subsystemQueue, &statusMsg, portMAX_DELAY);
    if (qStatus == pdPASS)
    {
      Serial.println(statusMsg);
    }
  }
}

// Function to perform self-check
inline bool performSelfCheck()
{
  // Simulate self-check logic
  return true; // Replace with actual self-check logic
}

// Function to check ventilation status
inline bool ventilationCheck()
{
  // Simulate ventilation check logic
  return true; // Replace with actual ventilation check logic
}

// Function to perform fuel check
inline bool performFuelCheck()
{
  // Replace this with your actual fuel check logic
  return true; // Placeholder, modify as needed
}

// Function to retrieve fuel level
inline float fuelLevel()
{
  // Replace this with your actual fuel level retrieval logic
  return 50.03; // Placeholder, modify as needed
}

// Main loop (not used in FreeRTOS)
void loop()
{
  // Empty loop, tasks will run in the FreeRTOS scheduler
}
