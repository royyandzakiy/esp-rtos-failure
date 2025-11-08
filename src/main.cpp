#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>
#include <esp_heap_caps.h>
#include <cstring>

class RTOSErrorSimulator {
private:
    TimerHandle_t xErrorTimer;
    SemaphoreHandle_t xMutex, xMutexA, xMutexB;
    bool raceConditionEnabled;
    int *badPointer;
    int crashCounter;

public:
    void initialize() {
        crashCounter = 0;
        raceConditionEnabled = false;
        badPointer = nullptr;
        
        xMutex = xSemaphoreCreateMutex();
        xErrorTimer = xTimerCreate(
            "ErrorTimer",
            pdMS_TO_TICKS(10000), // 10 seconds
            pdTRUE,
            this,
            errorTimerCallback
        );
        xTimerStart(xErrorTimer, 0);
        
        Serial.println("RTOS Error Simulator Initialized");
        Serial.println("Commands: c=Immediate crash; w=Watchdog timeout; d=Deadlock; r=Race condition; s=stack; m=memory; s=Stack overflow; m=Memory corruption; p=Priority inversion;");
    }

    static void errorTimerCallback(TimerHandle_t xTimer) {
        RTOSErrorSimulator* simulator = (RTOSErrorSimulator*)pvTimerGetTimerID(xTimer);
        simulator->printCrashReport();
    }

    void simulateStackOverflow() {
        Serial.println("\n=== SIMULATING STACK OVERFLOW ===");
        
        // Method 1: Create task with insufficient stack
        xTaskCreatePinnedToCore(
            stackHungryTask,    // Task function
            "StackHungry",      // Name
            64,                 // WAY too small stack (should be 1024+)
            this,               // Parameter
            1,                  // Priority
            NULL,               // Handle
            0                   // Core
        );
        
        // Method 2: Recursive function to blow stack
        xTaskCreatePinnedToCore(
            recursiveStackBlower, 
            "StackBlower", 
            2048, 
            this, 
            1, 
            NULL, 
            1
        );
    }

    static void stackHungryTask(void *pvParameters) {
        Serial.println("StackHungryTask: Starting with tiny stack...");
        
        // Allocate large array on stack to cause immediate overflow
        char hugeBuffer[2048]; // This will overflow the 64-byte stack
        
        // This likely won't execute
        memset(hugeBuffer, 0xAA, sizeof(hugeBuffer));
        Serial.println("StackHungryTask: If you see this, stack wasn't overflowed");
        
        vTaskDelete(NULL);
    }

    static void recursiveStackBlower(void *pvParameters) {
        char buffer[256]; // Eat 256 bytes per recursion
        uint32_t depth = (uint32_t) pvParameters;
        
        if(depth % 10 == 0) {
            Serial.printf("Recursion depth: %d, Stack used: ~%d bytes\n", 
                         depth, depth * 256);
        }
        
        if(depth < 100) { // Should overflow before this
            recursiveStackBlower((void*) (depth + 1));
        }
        
        // Never reached
        Serial.println("Recursion completed - this shouldn't happen!");
    }

    void simulateMemoryCorruption() {
        Serial.println("\n=== SIMULATING MEMORY CORRUPTION ===");
        
        // Method 1: Use after free
        xTaskCreatePinnedToCore(useAfterFreeTask, "UseAfterFree", 2048, this, 1, NULL, 0);
        
        // Method 2: Buffer overflow
        xTaskCreatePinnedToCore(bufferOverflowTask, "BufferOverflow", 2048, this, 1, NULL, 1);
        
        // Method 3: Bad pointer access
        xTaskCreatePinnedToCore(badPointerTask, "BadPointer", 2048, this, 1, NULL, 0);
    }

    static void useAfterFreeTask(void *pvParameters) {
        RTOSErrorSimulator* sim = (RTOSErrorSimulator*)pvParameters;
        
        Serial.println("UseAfterFree: Allocating memory...");
        int *data = (int*)pvPortMalloc(sizeof(int) * 10);
        
        if(data) {
            for(int i = 0; i < 10; i++) {
                data[i] = i * 100;
            }
            
            Serial.println("UseAfterFree: Freeing memory...");
            vPortFree(data);
            
            vTaskDelay(pdMS_TO_TICKS(1000));
            
            Serial.println("UseAfterFree: Using after free (CRASH IMMINENT)...");
            // THIS WILL CRASH - using freed memory
            for(int i = 0; i < 10; i++) {
                Serial.printf("Data[%d] = %d\n", i, data[i]); // CRASH!
            }
        }
        
        vTaskDelete(NULL);
    }

    static void bufferOverflowTask(void *pvParameters) {
        Serial.println("BufferOverflow: Starting buffer overflow...");
        
        char smallBuffer[16];
        char secretData[45] = "This is secret data that might get corrupted";
        // char secretData[32] = "This is secret data that might get corrupted";
        
        Serial.printf("Before overflow - Secret: %s\n", secretData);
        
        // Deliberately overflow the buffer
        strcpy(smallBuffer, "This string is way too long for the buffer!");
        
        Serial.printf("After overflow - Secret: %s\n", secretData); // Likely corrupted
        Serial.println("BufferOverflow: If you see this, overflow didn't crash (but memory is corrupted)");
        
        vTaskDelete(NULL);
    }

    static void badPointerTask(void *pvParameters) {
        RTOSErrorSimulator* sim = (RTOSErrorSimulator*)pvParameters;
        
        Serial.println("BadPointer: Accessing bad memory locations...");
        
        // Try to access various bad pointers
        int *nullPtr = nullptr;
        int *wildPtr = (int*)0xDEADBEEF;
        int *invalidPtr = (int*)0x40000000; // Possibly invalid memory region
        
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        Serial.println("BadPointer: Attempting to write to null pointer...");
        *nullPtr = 42; // This should crash
        
        Serial.println("BadPointer: Survived null write, trying wild pointer...");
        *wildPtr = 1337; // This should also crash
        
        vTaskDelete(NULL);
    }

    void simulateRaceCondition() {
        Serial.println("\n=== SIMULATING RACE CONDITION ===");
        raceConditionEnabled = true;
        
        // Create multiple tasks accessing shared resource without protection
        for(int i = 0; i < 3; i++) {
            xTaskCreatePinnedToCore(
                raceConditionTask,
                String("RaceTask_" + String(i)).c_str(),
                2048,
                this,
                2, // Same priority = more race conditions
                NULL,
                i % 2
            );
        }
    }

    static void raceConditionTask(void *pvParameters) {
        RTOSErrorSimulator* sim = (RTOSErrorSimulator*)pvParameters;
        static int unsafeCounter = 0; // UNSAFE shared variable
        static int safeCounter = 0; // Protected version for comparison
        
        char taskName[16];
        strcpy(taskName, pcTaskGetName(NULL));
        
        for(int i = 0; i < 20; i++) {
            if(sim->raceConditionEnabled) {
                // UNSAFE: Direct access to shared variable
                int temp = unsafeCounter; // ANALOGY: OPEN TOILET DOOR
                vTaskDelay(pdMS_TO_TICKS(1)); // Increase race window
                unsafeCounter = temp + 1; // ANALOGY: POOP!!!
                
                // SAFE: Protected access for comparison
                if(xSemaphoreTake(sim->xMutex, portMAX_DELAY)) {
                    int temp2 = safeCounter; // ANALOGY: OPEN TOILET DOOR 
                    vTaskDelay(pdMS_TO_TICKS(1));
                    safeCounter = temp2 + 1; // ANALOGY: POOP!!!
                    xSemaphoreGive(sim->xMutex);
                }
                
                if(i % 5 == 0) {
                    Serial.printf("%s: Unsafe Counter=%d, Safe Counter=%d, Differences=%d\n", 
                                 taskName, unsafeCounter, safeCounter, 
                                 safeCounter - unsafeCounter);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        Serial.printf("%s FINAL: Unsafe Counter=%d, Safe Counter=%d, Lost Counts=%d updates\n", 
                     taskName, unsafeCounter, safeCounter, 
                     safeCounter - unsafeCounter);
        
        vTaskDelete(NULL);
    }

    void simulateDeadlock() {
        Serial.println("\n=== SIMULATING DEADLOCK ===");
        
        xTaskCreatePinnedToCore(deadlockTask1, "Deadlock1", 2048, this, 2, NULL, 0);
        xTaskCreatePinnedToCore(deadlockTask2, "Deadlock2", 2048, this, 2, NULL, 1);
    }

    static void deadlockTask1(void *pvParameters) {
        RTOSErrorSimulator* sim = (RTOSErrorSimulator*)pvParameters;
        Serial.println("Deadlock1: Attempting to take Mutex A then B...");
        
        if(xSemaphoreTake(sim->xMutexA, portMAX_DELAY)) {
            Serial.println("Deadlock1: Got Mutex A, waiting then taking Mutex B...");
            vTaskDelay(pdMS_TO_TICKS(100)); // Give task2 time to get Mutex B
            
            Serial.println("Deadlock1: Trying to take Mutex B (WILL DEADLOCK)...");
            if(xSemaphoreTake(sim->xMutexB, pdMS_TO_TICKS(5000))) {
                Serial.println("Deadlock1: Got both mutexes (unexpected!)");
                // Critical section work here
                xSemaphoreGive(sim->xMutexB);
                xSemaphoreGive(sim->xMutexA);
            } else {
                Serial.println("Deadlock1: Failed to get Mutex B (deadlock avoided?)");
                xSemaphoreGive(sim->xMutexA);
            }
        }
        
        vTaskDelete(NULL);
    }

    static void deadlockTask2(void *pvParameters) {
        RTOSErrorSimulator* sim = (RTOSErrorSimulator*)pvParameters;
        Serial.println("Deadlock2: Attempting to take Mutex B then A...");
        
        if(xSemaphoreTake(sim->xMutexB, portMAX_DELAY)) {
            Serial.println("Deadlock2: Got Mutex B, waiting then taking Mutex A...");
            vTaskDelay(pdMS_TO_TICKS(150)); // Give task1 time to get Mutex A
            
            Serial.println("Deadlock2: Trying to take Mutex A (WILL DEADLOCK)...");
            if(xSemaphoreTake(sim->xMutexA, pdMS_TO_TICKS(5000))) {
                Serial.println("Deadlock2: Got both mutexes (unexpected!)");
                // Critical section work here
                xSemaphoreGive(sim->xMutexA);
                xSemaphoreGive(sim->xMutexB);
            } else {
                Serial.println("Deadlock2: Failed to get Mutex A (deadlock avoided?)");
                xSemaphoreGive(sim->xMutexB);
            }
        }
        
        vTaskDelete(NULL);
    }

    void simulatePriorityInversion() {
        Serial.println("\n=== SIMULATING PRIORITY INVERSION ===");
        
        // Create low-priority task that holds a resource
        xTaskCreatePinnedToCore(
            lowPriorityTask, "LowPri", 2048, this, 1, NULL, 0
        );
        
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Create high-priority task that needs the same resource
        xTaskCreatePinnedToCore(
            highPriorityTask, "HighPri", 2048, this, 3, NULL, 0
        );
        
        // Create medium-priority task that doesn't need resource
        xTaskCreatePinnedToCore(
            mediumPriorityTask, "MedPri", 2048, this, 2, NULL, 1
        );
    }

    static void lowPriorityTask(void *pvParameters) {
        RTOSErrorSimulator* sim = (RTOSErrorSimulator*)pvParameters;
        
        Serial.println("LowPri: Taking mutex...");
        if(xSemaphoreTake(sim->xMutex, portMAX_DELAY)) {
            Serial.println("LowPri: Got mutex, doing long operation...");
            for(int i = 0; i < 10; i++) {
                Serial.printf("LowPri: Working %d/10\n", i + 1);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            Serial.println("LowPri: Releasing mutex");
            xSemaphoreGive(sim->xMutex);
        }
        
        vTaskDelete(NULL);
    }

    static void highPriorityTask(void *pvParameters) {
        RTOSErrorSimulator* sim = (RTOSErrorSimulator*)pvParameters;
        
        Serial.println("HighPri: Waiting for mutex (should get it immediately if no inversion)...");
        int64_t startTime = esp_timer_get_time();
        
        if(xSemaphoreTake(sim->xMutex, pdMS_TO_TICKS(15000))) {
            int64_t waitTime = (esp_timer_get_time() - startTime) / 1000;
            Serial.printf("HighPri: Got mutex after %lld ms (inversion occurred if >1000ms)\n", waitTime);
            vTaskDelay(pdMS_TO_TICKS(100));
            xSemaphoreGive(sim->xMutex);
        } else {
            Serial.println("HighPri: TIMEOUT - never got mutex!");
        }
        
        vTaskDelete(NULL);
    }

    static void mediumPriorityTask(void *pvParameters) {
        Serial.println("MedPri: Starting CPU-intensive work...");
        
        // This task will starve LowPri task of CPU time
        for(int i = 0; i < 8; i++) {
            Serial.printf("MedPri: Busy %d/8\n", i + 1);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        
        Serial.println("MedPri: Done");
        vTaskDelete(NULL);
    }

    void simulateWatchdogTimeout() {
        Serial.println("\n=== SIMULATING WATCHDOG TIMEOUT ===");
        
        xTaskCreatePinnedToCore(
            infiniteLoopTask, "InfiniteLoop", 2048, this, 1, NULL, 0
        );
    }

    static void infiniteLoopTask(void *pvParameters) {
        Serial.println("InfiniteLoop: Starting infinite loop (will trigger watchdog)...");
        
        // Disable warnings for the demonstration
        // In real code, this would reset the chip
        volatile int counter = 0;
        while(true) {
            counter++;
            if(counter % 1000000 == 0) {
                Serial.printf("InfiniteLoop: Still looping... %d\n", counter);
                // Note: Normally you'd call vTaskDelay or yield here
                // But we're deliberately not yielding to cause issues
            }
            // This will eventually trigger watchdog if not reset
        }
        
        vTaskDelete(NULL);
    }

    void printCrashReport() {
        crashCounter++;
        Serial.println("\n--- System Crash Report ---");
        Serial.printf("System uptime: %d cycles\n", crashCounter * 10);
        Serial.printf("Min free heap: %d bytes\n", heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
        Serial.printf("Tasks running: %d\n", uxTaskGetNumberOfTasks());
        
        // Check if any obvious issues
        if(heap_caps_get_free_size(MALLOC_CAP_8BIT) < 10000) {
            Serial.println("WARNING: Low memory condition!");
        }
        
        Serial.println("--- End Crash Report ---");
    }

    void handleSerialCommand(char cmd) {
        switch(cmd) {
            case 'w': simulateWatchdogTimeout(); break;
            case 'd': simulateDeadlock(); break;
            case 'r': simulateRaceCondition(); break;
            case 's': simulateStackOverflow(); break;
            case 'm': simulateMemoryCorruption(); break;
            case 'p': simulatePriorityInversion(); break;
            case 'c': 
                Serial.println("Manual crash triggered!");
                *((volatile int*)0) = 42; // Immediate segfault
                break;
            case 'h':
                Serial.println("Available commands:");
                Serial.println("c - Immediate crash");
                Serial.println("w - Watchdog timeout");
                Serial.println("d - Deadlock");
                Serial.println("r - Race condition");
                Serial.println("s - Stack overflow");
                Serial.println("m - Memory corruption");
                Serial.println("p - Priority inversion");
                break;
        }
    }
};

// Global simulator instance
RTOSErrorSimulator errorSimulator;

EventGroupHandle_t xEventGroup;

// Add to your existing setup() function:
void setup() {
    Serial.begin(115200);
    vTaskDelay(pdMS_TO_TICKS(3000)); // Wait for serial
    Serial.println("FreeRTOS Runtime Error Simulator");
    Serial.println("WARNING: This will cause system instability and crashes!");
    
    // Initialize error simulator (comment out for normal operation)
    errorSimulator.initialize();
    
    // Your normal application initialization here...
    xEventGroup = xEventGroupCreate();
    // Create your normal tasks...
}

void loop() {
    // Handle serial commands for error simulation
    if(Serial.available()) {
        char cmd = Serial.read();
        errorSimulator.handleSerialCommand(cmd);
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
}

// Add FreeRTOS hook functions to catch errors
extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    Serial.printf("\n!!! STACK OVERFLOW in task %s !!!\n", pcTaskName);
    // Don't reset - let user see the error
}

extern "C" void vApplicationMallocFailedHook(void) {
    Serial.println("\n!!! MALLOC FAILED - Out of memory !!!");
}