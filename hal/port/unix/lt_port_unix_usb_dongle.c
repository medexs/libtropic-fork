#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>

#include "libtropic_common.h"
#include "libtropic_port.h"


#define READ_WRITE_DELAY 10

int fd;
const char device[] = "/dev/ttyACM0";
uint32_t baud_rate = 115200;

// Writes bytes to the serial port, returning 0 on success and -1 on failure.
int write_port(int fd, uint8_t * buffer, size_t size)
{
  ssize_t result = write(fd, buffer, size);
  if (result != (ssize_t)size)
  {
    perror("failed to write to port");
    return -1;
  }
  return 0;
}

// Reads bytes from the serial port.
// Returns after all the desired bytes have been read, or if there is a
// timeout or other error.
// Returns the number of bytes successfully read into the buffer, or -1 if
// there was an error reading.
ssize_t read_port(int fd, uint8_t * buffer, size_t size)
{
  size_t received = 0;
  while (received < size)
  {
    ssize_t r = read(fd, buffer + received, size - received);
    if (r < 0)
    {
      perror("failed to read from port");
      return -1;
    }
    if (r == 0)
    {
      // Timeout
      break;
    }
    received += r;
  }
  return received;
}

lt_ret_t lt_port_init(lt_handle_t *h)
{
    UNUSED(h);
    memset(h, 0, sizeof(lt_handle_t));

    // serialport init
    fd = open(device, O_RDWR | O_NOCTTY);
    if (fd == -1)
    {
      perror(device);
      return LT_FAIL;
    }

    // Flush away any bytes previously read or written.
    int result = tcflush(fd, TCIOFLUSH);
    if (result)
    {
      perror("tcflush failed");  // just a warning, not a fatal error
    }

    // Get the current configuration of the serial port.
    struct termios options;
    result = tcgetattr(fd, &options);
    if (result)
    {
      perror("tcgetattr failed");
      close(fd);
      return LT_FAIL;
    }

    // Turn off any options that might interfere with our ability to send and
    // receive raw binary bytes.
    options.c_iflag &= ~(INLCR | IGNCR | ICRNL | IXON | IXOFF);
    options.c_oflag &= ~(ONLCR | OCRNL);
    options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

    // Set up timeouts: Calls to read() will return as soon as there is
    // at least one byte available or when 100 ms has passed.
    options.c_cc[VTIME] = 1;
    options.c_cc[VMIN] = 0;

    // This code only supports certain standard baud rates. Supporting
    // non-standard baud rates should be possible but takes more work.
    switch (baud_rate)
    {
    case 4800:   cfsetospeed(&options, B4800);   break;
    case 9600:   cfsetospeed(&options, B9600);   break;
    case 19200:  cfsetospeed(&options, B19200);  break;
    case 38400:  cfsetospeed(&options, B38400);  break;
    case 115200: cfsetospeed(&options, B115200); break;
    default:
        fprintf(stderr, "warning: baud rate %u is not supported, using 9600.\n",
            baud_rate);
        cfsetospeed(&options, B9600);
        break;
    }
    cfsetispeed(&options, cfgetospeed(&options));

    result = tcsetattr(fd, TCSANOW, &options);
    if (result)
    {
        perror("tcsetattr failed");
        close(fd);
        return LT_FAIL;
    }

    return LT_OK;
}

lt_ret_t lt_port_deinit(lt_handle_t *h)
{
    UNUSED(h);
    close(fd);

    return LT_OK;
}

lt_ret_t lt_port_delay (lt_handle_t *h, uint32_t wait_time_msecs)
{
    UNUSED(h);
    usleep(wait_time_msecs*1000);
    return LT_OK;
}

lt_ret_t lt_port_random_bytes(uint32_t *buff, uint16_t len) {

    for(int i=0; i<len; i++) {
        buff[i] = (uint32_t)rand();
    }

    return LT_OK;
}


lt_ret_t lt_port_spi_csn_low (lt_handle_t *h)
{
    UNUSED(h);
    /* CS LOW is handled automatically when SPI transfer is executed */
    return LT_OK;
}

lt_ret_t lt_port_spi_csn_high (lt_handle_t *h)
{
    UNUSED(h);

    char cs_high[] = "CS=0\n"; // Yes, CS=0 really means that CSN is low
    if(write_port(fd, cs_high, 5) != 0) {
        return LT_L1_SPI_ERROR;
    }

    uint8_t buff[4];
    int readed = read_port(fd, buff, 4);
    if(readed != 4) {
        return LT_L1_SPI_ERROR;
    }

    if(memcmp(buff, "OK\r\n", 4) !=0) {
        return LT_L1_SPI_ERROR;
    }
    return LT_OK;
}

lt_ret_t lt_port_spi_transfer (lt_handle_t *h, uint8_t offset, uint16_t tx_data_length, uint32_t timeout)
{
    UNUSED(h);
    UNUSED(timeout);

    if (offset + tx_data_length > LT_L1_LEN_MAX) {
        return LT_L1_DATA_LEN_ERROR;
    }

    // Bytes from handle which are about to be sent are encoded as chars and stored here:
    char buffered_chars[512] = {0};
    for(int i=0; i<(tx_data_length); i++) {
        sprintf(buffered_chars + i*2, "%02X", h->l2_buff[i+offset]); // TODO make this better?
    }
    // Controll characters to keep CS LOW, they are expected by USB dongle
    buffered_chars[tx_data_length*2] = 'x';
    buffered_chars[tx_data_length*2+1] = '\n';


    int ret = write_port(fd, buffered_chars, (tx_data_length*2)+2);
    if(ret != 0) {
        return LT_L1_SPI_ERROR;
    }

    lt_port_delay(h, READ_WRITE_DELAY);

    int readed = read_port(fd, buffered_chars, (2*tx_data_length)+2);
    if(readed!= ((2*tx_data_length)+2)) {
        return LT_L1_SPI_ERROR;
    }

    for (size_t count = 0; count < 2*tx_data_length; count++) {
        sscanf(&buffered_chars[count*2], "%2hhx", &h->l2_buff[count+offset]); // TODO make this better?
    }

    return LT_OK;
}
