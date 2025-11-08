# FreeRTOS Runtime Error Simulator

This project implements a comprehensive runtime error simulation framework for FreeRTOS on ESP32 using the Arduino framework. It provides controlled demonstrations of common RTOS failure scenarios for educational and testing purposes.

## Overview

The simulator systematically demonstrates critical FreeRTOS failure modes through controlled experiments:

1. **Watchdog Timeouts** - Infinite loops and unresponsive tasks
2. **Deadlocks** - Circular dependency in resource acquisition
3. **Race Conditions** - Unsynchronized access to shared resources
4. **Stack Overflow** - Task stack exhaustion through recursion and insufficient allocation
5. **Memory Corruption** - Use-after-free, buffer overflow, and invalid pointer access
6. **Priority Inversion** - High-priority task blocking on lower-priority tasks

## Operational Context

This framework serves as a diagnostic and educational tool for understanding FreeRTOS failure modes in embedded systems. Each simulation is contained and documented to demonstrate specific failure mechanisms.

## Implementation Architecture

### Core Components

- **RTOSErrorSimulator Class** - Central controller managing all error simulations
- **Timer-Based Monitoring** - Periodic system health reporting
- **Controlled Failure Injection** - Isolated fault scenarios
- **Serial Command Interface** - Runtime simulation control

### Error Simulation Modules

#### Stack Overflow
- Insufficient stack allocation during task creation
- Recursive function calls exhausting stack space
- Real-time stack usage reporting

#### Memory Corruption
- Use-after-free scenarios with dynamic allocation
- Buffer overflow across stack boundaries
- Invalid memory region access attempts

#### Concurrency Issues
- Race conditions through unsynchronized shared access
- Deadlock through circular mutex dependencies
- Priority inversion demonstration with three-task interaction

#### System Failures
- Watchdog timeout through non-yielding loops
- Manual crash injection for immediate failure testing

## Usage Protocol

1. Open PlatformIO project in VSCode
2. Connect ESP32 development board
3. Build and upload firmware
4. Open serial monitor at 115200 baud
5. Execute simulation commands via serial interface

### Command Reference
- `c` - Immediate system crash
- `w` - Watchdog timeout trigger
- `d` - Deadlock simulation
- `r` - Race condition demonstration
- `s` - Stack overflow simulation
- `m` - Memory corruption scenarios
- `p` - Priority inversion test

- `h` - Help: Command reference display

## Diagnostic Features

### System Monitoring
- Periodic crash reporting via timer callback
- Heap usage tracking and minimum free memory
- Task count monitoring
- Stack overflow hook integration

### Error Detection
- FreeRTOS hook functions for stack overflow detection
- Malloc failure monitoring
- Real-time corruption detection
- Deadlock timeout mechanisms

## Technical Implementation

### Task Management
Tasks are created with specific stack sizes and core assignments to demonstrate resource constraints. Each simulation runs in isolated tasks to contain failures.

### Memory Management
Heap allocation tracking through ESP32-specific functions demonstrates memory fragmentation and exhaustion scenarios.

### Synchronization Primitives
Mutexes and semaphores are used both correctly and incorrectly to demonstrate proper and improper synchronization patterns.

## Observational Output

The system provides detailed serial output showing:
- Pre-failure system state
- Failure progression metrics
- Post-failure analysis where possible
- Comparative behavior between correct and incorrect implementations

## Development Context

This implementation serves as both an educational demonstration and a testing framework for understanding FreeRTOS failure modes. Each simulation is designed to be reproducible and observable while maintaining system stability where possible.

The code structure allows for extension with additional failure scenarios and provides a foundation for robustness testing in FreeRTOS-based embedded systems.