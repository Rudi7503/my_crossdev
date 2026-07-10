/*
 * I2C Bluetooth Device Master
 * ApolloCrossDev I2C-test Project
 *
 * This program implements the I2C master side for communicating with
 * an ESP32 I2C slave at address 0x50. The protocol allows:
 * - Reading available Bluetooth devices from the ESP32
 * - Selecting a device by sending its index
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <proto/exec.h>
#include <proto/i2c.h>
#include <libraries/i2c.h>

#define ESP32_I2C_ADDR 0xA0
#define MAX_DEVICES 10
#define MAX_DEVICE_STRING 256

struct Library *I2C_Base;

void cleanup(void) {
    if (I2C_Base) {
        CloseLibrary(I2C_Base);
    }
}

int main(int argc, char *argv[]) {
    UBYTE device_list[MAX_DEVICE_STRING];
    UBYTE selected_index;
    ULONG err;
    int choice;

    printf("I2C Bluetooth Device Master\n");
    printf("===========================\n\n");

    // Open I2C library
    I2C_Base = OpenLibrary("i2c.library", 39);
    if (!I2C_Base) {
        printf("Error: Cannot open i2c.library V39+\n");
        return 10;
    }

    // Set up cleanup
    atexit(cleanup);

    char input[32];
    int device_count = 0;

    while (1) {
        printf("Reading available Bluetooth devices from ESP32...\n");

        // Read device list from ESP32 (address 0x50)
        err = ReceiveI2C(ESP32_I2C_ADDR, MAX_DEVICE_STRING, device_list);
    /*    if ((err & 0xff) != 0) {
            printf("I2C Error: %s\n", I2CErrText(err));
            return 10;
        }
*/
        // Null-terminate the string
        device_list[MAX_DEVICE_STRING - 1] = '\0';

        // Check if no devices found
        if (strcmp((char*)device_list, "No devices found") == 0) {
            printf("No Bluetooth devices found.\n");
            device_count = 0;
        } else {
            printf("Available Bluetooth devices:\n");
            printf("----------------------------\n");

            // Parse and display device list
            char *token = strtok((char*)device_list, "\n");
            device_count = 0;

            while (token != NULL && device_count < MAX_DEVICES) {
                printf("%d: %s\n", device_count, token);
                device_count++;
                token = strtok(NULL, "\n");
            }

            if (device_count == 0) {
                printf("No devices found in response.\n");
            }
        }

        printf("\nOptions:\n");
        printf("  r - request the list again\n");
        printf("  s - select a device\n");
        printf("  q - quit\n");
        printf("Choice: ");

        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("Input error.\n");
            return 5;
        }

        if (input[0] == 'q' || input[0] == 'Q') {
            return 0;
        }

        if (input[0] == 'r' || input[0] == 'R') {
            // Loop again to re-query the device list without sending selection
            printf("\nRe-requesting device list...\n\n");
            continue;
        }

        if (input[0] == 's' || input[0] == 'S') {
            if (device_count == 0) {
                printf("Keine Geräte zum Auswählen vorhanden. Bitte erneut abfragen.\n\n");
                continue;
            }

            printf("\nSelect a device (0-%d): ", device_count - 1);
            if (fgets(input, sizeof(input), stdin) == NULL) {
                printf("Input error.\n");
                return 5;
            }

            if (sscanf(input, "%d", &choice) != 1) {
                printf("Invalid input.\n\n");
                continue;
            }

            if (choice < 0 || choice >= device_count) {
                printf("Invalid device selection.\n\n");
                continue;
            }

            selected_index = (UBYTE)choice;
            break;
        }

        printf("Unknown choice. Bitte 'r', 's' oder 'q' eingeben.\n\n");
    }

    printf("Connecting to device %d...\n", choice);

    // Send selected device index to ESP32
    err = SendI2C(ESP32_I2C_ADDR, 1, &selected_index);

    if ((err & 0xff) != 0) {
        printf("I2C Error sending selection: %s\n", I2CErrText(err));
        return 10;
    }

    printf("Device selection sent successfully!\n");
    printf("ESP32 should now connect to the selected Bluetooth device.\n");

    return 0;
}