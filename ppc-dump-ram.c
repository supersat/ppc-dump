#include <stdio.h>
#include <time.h>
#include <ftdi.h>

int main(int argc, char *argv[])
{
    struct ftdi_context *ftdi;
    unsigned char buf[64];
    unsigned int i, addr;
    int retVal;
    struct timespec startTime, curTime;
    long int elapsed;

    ftdi = ftdi_new();
    if (!ftdi) {
        fprintf(stderr, "ftdi_new() failed\n");
        return EXIT_FAILURE;
    }

    /*
    desc: C232HM-DDHSL-0
    serial: FTWH8RBI
    */

    if (ftdi_usb_open_desc(ftdi, 0x0403, 0x6014, "C232HM-DDHSL-0", NULL) < 0) {
        fprintf(stderr, "Unable to open FTDI device: %s\n", ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "Opened FTDI device.\n");

    ftdi_set_event_char(ftdi, 0, 0);
    ftdi_set_error_char(ftdi, 0, 0);
    ftdi_set_latency_timer(ftdi, 1);
    ftdi_setflowctrl(ftdi, SIO_RTS_CTS_HS);


    if (ftdi_set_bitmode(ftdi, 0, 0) < 0) {
        fprintf(stderr, "ftdi_set_bitmode failed: %s", ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "Reset FTDI device.\n");

    if (ftdi_set_bitmode(ftdi, 0, BITMODE_MPSSE) < 0) {
        fprintf(stderr, "ftdi_set_bitmode failed: %s", ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "MPSSE mode initialized\n");

    ftdi_usb_purge_rx_buffer(ftdi);

    // Enable divide by 5, disable adaptive and three-phase clocking
    ftdi_write_data(ftdi, "\x8b\x97\x8d", 3);
    // Set clk divisor
    ftdi_write_data(ftdi, "\x86\x04\x00", 3);

    for (i = 0; i < 0x800000; ) {
        // Configure pin directions and set DSCK high
        ftdi_write_data(ftdi, "\x80\x01\x03", 3);
        fprintf(stderr, "Waiting for PPC reset... ");

        // Wait for SRESET to be asserted
        do {
            ftdi_write_data(ftdi, "\x81", 1);
            do {
                retVal = ftdi_read_data(ftdi, buf, 1);
            } while (retVal < 1);
        } while (buf[0] & 0x10);

        // Wait for SRESET to be released
        do {
            ftdi_write_data(ftdi, "\x81", 1);
            do {
                retVal = ftdi_read_data(ftdi, buf, 1);
            } while (retVal < 1);
        } while (!(buf[0] & 0x10));

        fprintf(stderr, "Reset detected!\n");
        clock_gettime(CLOCK_MONOTONIC, &startTime);

        // Release DSCK
        usleep(50000);
        ftdi_write_data(ftdi, "\x80\x00\x03", 3);
        usleep(50000);
        fprintf(stderr, "Dumping... (0x%08x bytes so far)\n", i);

        // These two commented instructions resume the CPU and are only here for testing
        //ftdi_write_data(ftdi, "\x11\x04\x00\x04\x7f\xf4\x22\xa6", 8);
        //ftdi_write_data(ftdi, "\x11\x04\x00\x04\x4c\x00\x00\x64", 8);
        /*
        // FOR TESTING
        ftdi_write_data(ftdi, "\x11\x04\x00\x04\x38\x20\x00\x17", 8);
        ftdi_write_data(ftdi, "\x11\x04\x00\x04\x38\x40\x00\x25", 8);
        ftdi_write_data(ftdi, "\x11\x04\x00\x04\x7f\xe1\x12\x14", 8);
        ftdi_write_data(ftdi, "\x11\x04\x00\x04\x7f\xf6\x9b\xa6\x31\x04\x00\x04\x60\x00\x00\x00", 16);
        do {
            retVal = ftdi_read_data(ftdi, buf, 5);
            fprintf(stderr, " retval = %d\n", retVal);
        } while (retVal < 4);

        fprintf(stderr, "Data read: %02x %02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
        */

        // lis r3, 0xff7f
        //addr = 0xff800000 + i - 4;
        addr = 0x00000000 + i - 4;
        buf[0] = 0x11;
        buf[1] = 0x04;
        buf[2] = 0x00;
        buf[3] = 0x04;
        buf[4] = 0x3c;
        buf[5] = 0x60;
        buf[6] = (addr & 0xff000000) >> 24;
        buf[7] = (addr & 0xff0000) >> 16;
        ftdi_write_data(ftdi, buf, 8);
        // ori r3, r3, 0xffff
        buf[4] = 0x60;
        buf[5] = 0x63;
        buf[6] = (addr & 0xff00) >> 8;
        buf[7] = (addr & 0xff);
        ftdi_write_data(ftdi, buf, 8);

        while (i < 0x800000) {
            clock_gettime(CLOCK_MONOTONIC, &curTime);
            if (curTime.tv_nsec < startTime.tv_nsec) {
                curTime.tv_sec--;
                curTime.tv_nsec += 1000000000;
            }

            elapsed = ((curTime.tv_sec - startTime.tv_sec) * 1000000000) + (curTime.tv_nsec - startTime.tv_nsec);
            if (elapsed > 2500000000) {
                fprintf(stderr, "2.5 seconds elapsed %ld\n", elapsed);
                break;
            }

            // lwzu r4, 4(r3)
            ftdi_write_data(ftdi, "\x11\x04\x00\x04\x84\x83\x00\x04", 8);
            // mtspr DPDR, r4
            ftdi_write_data(ftdi, "\x11\x04\x00\x04\x7c\x96\x9b\xa6", 8);

            // Read out DPDR
            ftdi_write_data(ftdi, "\x31\x04\x00\x04\x60\x00\x00\x00", 8);
            do {
                retVal = ftdi_read_data(ftdi, buf, 5);
            } while (retVal == 0);

            if (retVal != 5) {
                fprintf(stderr, "WARNING! ftdi_read_data did not return 5, but %d\n", retVal);
            }

            printf("%02x %02x %02x %02x\n", buf[1], buf[2], buf[3], buf[4]);
            i+= 4;
        }
    }

    ftdi_free(ftdi);
}
