# file_modem
Basic file transmission functions for embedded systems

Currently only x-modem is implemented. The usage is rather simple and straight forward. Currently it is set up to be used with FATFS (should be easy to modify to any other FS library).

Example:
```C
// Initialize the filemodem library
file_modem_init(&fm_recByte, &fm_sendByte, &fm_flushRx);

...
FRESULT fr; // FATFS Result
FIL fdst;   // opened FATFS File
uint32_t maxBytesToReceive = (1024*1024);   // Maximum amount of bytes we wanna receive
uint8_t fmr;    // xmodem result
...
fr = f_open(&fdst, argv[1], FA_CREATE_NEW | FA_WRITE);
// Handle error conditions for f_open
...
fmr = xmodem_receive(&fdst, &maxBytesToReceive);
f_close(&fdst);
// Handle errors for xmodem_receive
if (fmr)
{
    // Process the error, maybe delete what has been written already as it is incomplete
    ...
}
else
{
    // Transmission finished successful!
    printf("File received, %lu Bytes saved!\r\n",maxBytesToReceive);
}
```
