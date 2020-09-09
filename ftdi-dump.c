#include <stdio.h>
#include <ftdi.h>

int main(int argc, char *argv[])
{
    struct ftdi_context *ftdi;
    //struct ftdi_version_info version;
    struct ftdi_device_list *devlist;
    int numDevs, ret, i;
    char mfr[64], desc[64], serial[64];

    ftdi = ftdi_new();
    if (!ftdi) {
        fprintf(stderr, "ftdi_new() failed\n");
        return EXIT_FAILURE;
    }

    /*
    version = ftdi_get_library_version();
    printf("Initialized libftdi %s", version.version_str);
    */

    if ((numDevs = ftdi_usb_find_all(ftdi, &devlist, 0x0403, 0x6014)) < 0) {
        fprintf(stderr, "Unable to scan for FTDI devices: %s\n", ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
        return EXIT_FAILURE;
    }

    printf("Found %d devices\n", numDevs);
    i = 0;

    for (; devlist; devlist = devlist->next) {
        if (ftdi_usb_get_strings(ftdi, devlist->dev, mfr, sizeof(mfr), desc, sizeof(desc), serial, sizeof(serial)) < 0) {
            fprintf(stderr, "Unable to get strings for device %d: %s\n", i, ftdi_get_error_string(ftdi));
        }

        printf("%d - mfr: %s\n", i, mfr);
        printf("%d - desc: %s\n", i, desc);
        printf("%d - serial: %s\n", i, serial);
        i++;
    }
}
