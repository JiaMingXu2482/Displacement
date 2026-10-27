#include "stream_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    uint8_t out[STREAM_TEXT_BYTES + 1U];
    const int16_t example[] = {7500, 7508, 7515, 0, -3, 15000, -32768, 32767};
    const char *expected = "7500\r\n7508\r\n7515\r\n0\r\n-3\r\n15000\r\n-32768\r\n32767\r\n";
    int16_t worst[STREAM_BLOCK_SAMPLES];
    uint16_t i, length;
    memset(out, 0xA5, sizeof(out));
    length = Stream_FormatText(out, STREAM_TEXT_BYTES, example, 8U);
    assert(length == strlen(expected));
    assert(memcmp(out, expected, length) == 0);
    assert(out[length] == 0xA5);
    for (i = 0; i < STREAM_BLOCK_SAMPLES; i++) worst[i] = -32768;
    length = Stream_FormatText(out, STREAM_TEXT_BYTES, worst, STREAM_BLOCK_SAMPLES);
    assert(length == STREAM_TEXT_BYTES);
    assert(out[STREAM_TEXT_BYTES] == 0xA5);
    assert(Stream_FormatText(out, 1U, example, 1U) == 0U);
    assert(Stream_FormatText(out, STREAM_TEXT_BYTES, example, 65U) == 0U);
    assert(Stream_FormatText(NULL, STREAM_TEXT_BYTES, example, 1U) == 0U);
    assert(Stream_FormatText(out, STREAM_TEXT_BYTES, NULL, 1U) == 0U);
    puts("Text stream: values, signs, boundaries and buffer limits passed");
    return 0;
}
