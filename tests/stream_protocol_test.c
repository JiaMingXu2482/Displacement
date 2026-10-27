#include "stream_protocol.h"
#include <assert.h>
#include <stdio.h>
int main(int argc, char **argv)
{
    uint8_t packet[STREAM_PACKET_BYTES];
    int16_t positions[STREAM_BLOCK_SAMPLES];
    uint16_t length;
    unsigned i;
    FILE *file;
    assert(Stream_Crc16((const uint8_t *)"123456789", 9U) == 0x29B1U);
    assert(Stream_Pack(NULL, 0, 0, positions, 1) == 0);
    assert(Stream_Pack(packet, 0, 0, NULL, 1) == 0);
    assert(Stream_Pack(packet, 0, 0, positions, 65) == 0);
    for (i = 0; i < STREAM_BLOCK_SAMPLES; i++) positions[i] = (int16_t)((int)i * 300 - 3750);
    positions[0] = -32768; positions[1] = 32767;
    length = Stream_Pack(packet, 0xFFFFFFC0U, 0, positions, STREAM_BLOCK_SAMPLES);
    assert(length == 146 && packet[4] == 0xC0 && packet[7] == 0xFF);
    assert(packet[16] == 0 && packet[17] == 0x80 && packet[18] == 0xFF && packet[19] == 0x7F);
    assert(argc == 2);
    file = fopen(argv[1], "wb"); assert(file != NULL);
    assert(fwrite(packet, 1, length, file) == length);
    length = Stream_Pack(packet, 0U, 0, positions, STREAM_BLOCK_SAMPLES);
    assert(fwrite(packet, 1, length, file) == length);
    length = Stream_Pack(packet, 64U, 3, NULL, 0);
    assert(length == 18);
    assert(fwrite(packet, 1, length, file) == length);
    fclose(file);
    puts("C packet/CRC tests passed");
    return 0;
}
