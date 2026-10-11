#include "Compression.h"

// Original PC v1.2 0x573700: JDLZ decompression with separate literal/match and
// match-kind flag streams. Returns the uncompressed size, or zero for other data.
int JdlzDecompress(int *source, unsigned char *output) {
    if (source[0] != 0x5a4c444a || (char)source[1] != 2) {
        return 0;
    }
    unsigned char *end = output + source[2];
    unsigned char *input = reinterpret_cast<unsigned char *>(source) + 0x12;
    unsigned short literalFlags = *(reinterpret_cast<unsigned char *>(source) + 0x10) | 0x100;
    unsigned short kindFlags = *(reinterpret_cast<unsigned char *>(source) + 0x11) | 0x100;
    int remaining = source[3] - 0x12;
    while (remaining != 0) {
        if ((literalFlags & 1) == 0) {
            if (output < end) {
                *output++ = *input++;
            }
            remaining = remaining - 1;
        } else {
            unsigned char second = input[1];
            unsigned char first = input[0];
            int length;
            if ((kindFlags & 1) == 0) {
                length = (first & 0x1f) + 3;
                int distance = (((first & 0xe0) << 3) | second) + 0x11;
                for (int i = 0; i < length; ++i) {
                    output[i] = output[i - distance];
                }
            } else {
                length = (((first & 0xf0) << 4) | second) + 3;
                int distance = (first & 0xf) + 1;
                for (int i = 0; i < length; ++i) {
                    output[i] = output[i - distance];
                }
            }
            kindFlags = (unsigned short)((short)kindFlags >> 1);
            input += 2;
            remaining = remaining - 2;
            output += length;
        }
        literalFlags = (unsigned short)((short)literalFlags >> 1);
        if (literalFlags == 1) {
            literalFlags = *input++ | 0x100;
            remaining = remaining - 1;
        }
        if (kindFlags == 1) {
            kindFlags = *input++ | 0x100;
            remaining = remaining - 1;
        }
    }
    return source[2];
}

// Original 0x574630.
int bIsCompressed(unsigned int *source) {
    unsigned int tag = source[0];
    if (tag == 0x57574152 || tag == 0x46465548 || tag == 0x504d4f43) {
        return (char)source[1] == 1;
    }
    if (tag == 0x5a4c444a) {
        return (char)source[1] == 2;
    }
    return 0;
}

namespace {

// Bit reader state of 0x573840: the top of `bits` holds 16 + count unread bits and
// `reservoir` keeps the last two big-endian input words.
struct HuffBits {
    const unsigned char *input;
    unsigned int reservoir;
    unsigned int bits;
    int count;
};

inline void HuffRefill(HuffBits &state) {
    if (state.count < 0) {
        state.reservoir = (state.reservoir << 8 | state.input[0]) << 8 | state.input[1];
        state.input += 2;
        state.bits = state.reservoir << (-state.count & 0x1f);
        state.count += 0x10;
    }
}

inline void HuffSkip(HuffBits &state, int count) {
    state.bits <<= count;
    state.count -= count;
    HuffRefill(state);
}

// Variable-length count, inlined three times in the original.
unsigned int HuffReadCount(HuffBits &state) {
    unsigned int value = 0;
    if ((int)state.bits < 0) {
        value = state.bits >> 0x1d;
        HuffSkip(state, 3);
        return value - 4;
    }
    int length = 2;
    if ((state.bits & 0xffff0000) == 0) {
        // Slow path counts one more than the fast path for the same prefix, as in 0x573b50.
        do {
            ++length;
            value = state.bits >> 0x1f;
            HuffSkip(state, 1);
        } while (value == 0);
    } else {
        unsigned int previous;
        do {
            previous = state.bits;
            state.bits = previous << 1;
            ++length;
        } while ((int)state.bits >= 0);
        state.count += 1 - length;
        state.bits = previous << 2;
        if (huffExtraBits != 0) {
            value = state.bits >> ((0x20 - huffExtraBits) & 0x1f);
            state.bits <<= huffExtraBits & 0x1f;
            state.count -= huffExtraBits;
        }
        HuffRefill(state);
    }
    if (length < 0x11) {
        if (length != 0) {
            value = state.bits >> ((0x20 - length) & 0x1f);
            state.bits <<= length & 0x1f;
            state.count -= length;
        }
        HuffRefill(state);
    } else {
        if (length != 0x10) {
            value = state.bits >> ((0x30 - length) & 0x1f);
            state.bits <<= (length - 0x10) & 0x1f;
            state.count += 0x10 - length;
        }
        HuffRefill(state);
        unsigned int low = state.bits >> 0x10;
        HuffSkip(state, 0x10);
        value = value << 0x10 | low;
    }
    return (value - 4) + (1u << (length & 0x1f));
}

}  // namespace

// Original 0x573840: EA Huffman decoder with run escapes and optional delta passes.
// The original receives `input` in EAX.
unsigned int HuffDecode(const unsigned char *input, unsigned char *output) {
    if (input == 0) {
        return 0;
    }
    HuffBits state;
    state.input = input;
    state.reservoir = 0;
    state.bits = 0;
    state.count = -0x10;
    if (huffExtraBits != 0) {
        state.count = -0x10 - huffExtraBits;
    }
    HuffRefill(state);
    unsigned int type = state.bits >> 0x10;
    unsigned int top = state.bits;
    HuffSkip(state, 0x10);
    unsigned int high;
    if ((int)top < 0) {
        if (type & 0x100) {
            HuffSkip(state, 0x10);
            HuffSkip(state, 0x10);
        }
        high = state.bits >> 0x10;
        state.bits <<= 0x10;
        state.count -= 0x10;
    } else {
        if (type & 0x100) {
            HuffSkip(state, 8);
            HuffSkip(state, 0x10);
        }
        high = state.bits >> 0x18;
        state.bits <<= 8;
        state.count -= 8;
    }
    type &= ~0x100u;
    HuffRefill(state);
    unsigned int low = state.bits >> 0x10;
    HuffSkip(state, 0x10);
    unsigned int size = low | high << 0x10;
    unsigned char escape = (unsigned char)(state.bits >> 0x18);
    HuffSkip(state, 8);

    int base[0x22];
    unsigned int counts[0x22];
    unsigned int limits[0x22];
    int code = 0;
    int total = 0;
    int length = 1;
    unsigned int count;
    unsigned int limit;
    do {
        base[length] = code * 2 - total;
        count = HuffReadCount(state);
        counts[length] = count;
        code = code * 2 + count;
        total += count;
        limit = 0;
        if (count != 0) {
            limit = (code << ((0x10 - length) & 0x1f)) & 0xffff;
        }
        limits[length + 1] = limit;
        ++length;
    } while (count == 0 || limit != 0);
    int maximum = length - 1;
    limits[length] = 0xffffffff;

    unsigned char symbols[0x100];
    unsigned char order[0x100];
    for (int i = 0; i < 0x100; ++i) {
        symbols[i] = 0;
    }
    unsigned char current = 0xff;
    for (int i = 0; i < total; ++i) {
        int skip = (int)HuffReadCount(state) + 1;
        do {
            ++current;
            if (symbols[current] == 0) {
                --skip;
            }
        } while (skip != 0);
        symbols[current] = 1;
        order[i] = current;
    }

    unsigned char lengths[0x100];
    for (int i = 0; i < 0x100; ++i) {
        lengths[i] = 0x40;
    }
    unsigned char *symbolOut = symbols;
    unsigned char *lengthOut = lengths;
    const unsigned char *next = order;
    int escapeLength = 0;
    for (int bitsLength = 1, shift = 7; bitsLength <= maximum; ++bitsLength, --shift) {
        if (shift < 0) {
            break;
        }
        unsigned int span = 1u << shift;
        for (unsigned int k = counts[bitsLength]; k != 0; --k) {
            unsigned char symbol = *next++;
            unsigned char entry = (unsigned char)bitsLength;
            if (symbol == escape) {
                escapeLength = bitsLength;
                entry = 0x60;
            }
            for (unsigned int j = 0; j < span; ++j) {
                lengthOut[j] = entry;
                symbolOut[j] = symbol;
            }
            symbolOut += span;
            lengthOut += span;
        }
    }

    unsigned char *start = output;
    for (;;) {
        unsigned int index = state.bits >> 0x18;
        unsigned int entry = lengths[index];
        state.count -= entry;
        while (state.count >= 0) {
            state.bits <<= entry & 0x1f;
            *output++ = symbols[index];
            index = state.bits >> 0x18;
            entry = lengths[index];
            state.count -= entry;
        }
        state.count += 0x10;
        if (state.count >= 0) {
            *output++ = symbols[state.bits >> 0x18];
            state.reservoir = (state.reservoir << 8 | state.input[0]) << 8 | state.input[1];
            state.input += 2;
            state.bits = state.reservoir << ((0x10 - state.count) & 0x1f);
            continue;
        }
        int bitsLength = escapeLength;
        if (entry != 0x60) {
            bitsLength = 8;
            do {
                ++bitsLength;
            } while (limits[bitsLength + 1] <= state.bits >> 0x10);
        }
        unsigned int value = state.bits >> ((0x20 - bitsLength) & 0x1f);
        state.bits <<= bitsLength & 0x1f;
        unsigned char symbol = order[value - base[bitsLength]];
        state.count = ((entry - 0x10) + state.count) - bitsLength;
        if (symbol != escape) {
            if (state.count >= 0) {
                *output++ = symbol;
                continue;
            }
            HuffRefill(state);
            *output++ = symbol;
            continue;
        }
        HuffRefill(state);
        int run = (int)HuffReadCount(state);
        if (run == 0) {
            top = state.bits;
            HuffSkip(state, 1);
            if ((int)top < 0) {
                if (type == 0x32fb || type == 0xb2fb) {
                    unsigned char *end = start + size;
                    unsigned char sum = 0;
                    for (; start < end; ++start) {
                        sum = sum + *start;
                        *start = sum;
                    }
                } else if (type == 0x34fb || type == 0xb4fb) {
                    unsigned char *end = start + size;
                    char delta = 0;
                    unsigned char sum = 0;
                    for (; start < end; ++start) {
                        delta = delta + *start;
                        sum = sum + delta;
                        *start = sum;
                    }
                }
                return size;
            }
            unsigned char literal = (unsigned char)(state.bits >> 0x18);
            HuffSkip(state, 8);
            *output++ = literal;
        } else {
            unsigned char *end = output + run;
            unsigned char previous = output[-1];
            do {
                *output++ = previous;
            } while (output < end);
        }
    }
}

// Original 0x5744c0.
int HuffDecompress(int *source, unsigned char *output) {
    if (source[0] == 0x46465548 && (char)source[1] == 1) {
        HuffDecode(reinterpret_cast<unsigned char *>(source) + 0x10, output);
        return source[2];
    }
    return 0;
}

// Original 0x5744f0.
int RawDecompress(int *source, unsigned char *output) {
    if (source[0] == 0x57574152 && (char)source[1] == 1) {
        unsigned int size = source[2];
        const unsigned char *input = reinterpret_cast<unsigned char *>(source) + 0x10;
        for (unsigned int i = 0; i < size; ++i) {
            output[i] = input[i];
        }
        return source[2];
    }
    return 0;
}

// Original 0x574670 (the COMP decoder 0x574530 is not recovered).
int bDecompress(unsigned int *source, unsigned char *output) {
    if (!source || !output) {
        return 0;
    }
    switch (source[0]) {
    case 0x5a4c444a:
        return JdlzDecompress(reinterpret_cast<int *>(source), output);
    case 0x46465548:
        return HuffDecompress(reinterpret_cast<int *>(source), output);
    case 0x57574152:
        return RawDecompress(reinterpret_cast<int *>(source), output);
    }
    return 0;
}
