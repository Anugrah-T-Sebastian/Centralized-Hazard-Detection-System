#include "CO2Sensor.h"

CO2Sensor::CO2Sensor() : keepRunning(true) {
    gpioInitialise();
    gpioSetMode(LED_GPIO, PI_OUTPUT);
    gpioSetMode(BUZZER_GPIO, PI_OUTPUT);
    if (lcd1602Init(1, 0x3f) != 0) {
        std::cerr << "LCD initialization failed" << std::endl;
        gpioTerminate();
        std::exit(1);
    }
}

CO2Sensor::~CO2Sensor() {
    lcd1602Shutdown();
    gpioTerminate();
}

void CO2Sensor::startReadingThread() {
    sensorThread = std::thread([this]() {
        this->sensorReadingThread();
    });
}

void CO2Sensor::stopReadingThread() {
    keepRunning = false;
    sensorThread.join();
}

void CO2Sensor::initializeSensor() {
    sensirion_i2c_hal_init();
    scd4x_wake_up();
    scd4x_stop_periodic_measurement();
    scd4x_reinit();
    scd4x_start_periodic_measurement();
}

void CO2Sensor::updateDisplay(uint16_t co2, float temperature) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "CO2:%u ppm", co2);
    lcd1602SetCursor(0, 0);
    lcd1602WriteString(buffer);

    snprintf(buffer, sizeof(buffer), "Temp:%.2fC", temperature);
    lcd1602SetCursor(0, 1);
    lcd1602WriteString(buffer);
}

void CO2Sensor::sensorReadingThread() {
    initializeSensor();

    bool isFlashing = false; // Track if we are currently flashing the LED

    while (keepRunning) {
        bool data_ready_flag = false;
        sensirion_i2c_hal_sleep_usec(1000000); // 1 second delay
        scd4x_get_data_ready_flag(&data_ready_flag);
        if (!data_ready_flag) {
            continue;
        }
        uint16_t co2;
        float temperature, humidity;
        if (scd4x_read_measurement(&co2, &temperature, &humidity) == 0) {
            // Determine if we need to flash the LED
            bool shouldFlash = co2 > MAX_CO2 || temperature > MAX_TEMP;
            
            if (shouldFlash && !isFlashing) {
    // If we need to flash and are not already doing so, start flashing
                isFlashing = true;
                std::thread([&]() { // Capture by reference to allow the thread to see updates to isFlashing
           
                    while (isFlashing) {
                        gpioWrite(LED_GPIO, 1); // LED on
                        gpioWrite(BUZZER_GPIO, 1); // BUZZER on
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // delay 1000ms on
                        gpioWrite(LED_GPIO, 0); // LED off
                        gpioWrite(BUZZER_GPIO, 0); // Buzzer off
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // delay 1000ms off

                     
                    }
                    gpioWrite(LED_GPIO, 0); // Ensure the LED is off when the flashing stops
                    gpioWrite(BUZZER_GPIO, 0); // Ensure the BUZZER is off when the buzzing stops
                }).detach(); // Detach the thread so we don't need to join it later
            } else if (!shouldFlash) {
                // If we should stop flashing, update the flag
                isFlashing = false;
            }

            // Update the LCD display with the current CO2 and temperature readings
            updateDisplay(co2, temperature);
        }
    }

    isFlashing = false; // Ensure flashing stops when the loop exits
    gpioWrite(LED_GPIO, 0); // Ensure the LED is off when stopping

    scd4x_stop_periodic_measurement();
    sensirion_i2c_hal_free();
}

