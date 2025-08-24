#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP(x, low, high) (MIN((high), MAX((x), (low))))
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define SIGN(x) ((x) < 0 ? -1 : 1)
#define SWAP(a, b) { typeof(a) temp = a; a = b; b = temp; }

#define ARRAY_SET_VALUE(arr, value) memset((arr), value, sizeof(arr))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define ARRAY_END(arr) ((arr) + ARRAY_SIZE(arr))
#define ARRAY_FOREACH(arr, var) for (typeof(*(arr)) *var = (arr); var != ARRAY_END(arr); ++var)
#define ARRAY_FOREACH_REVERSE(arr, var) for (typeof(*(arr)) *var = ARRAY_END(arr) - 1; var != (arr) - 1; --var)
#define ARRAY_INDEX(arr, element) ((element) - (arr))
#define ARRAY_CONTAINS(arr, element) (ARRAY_INDEX(arr, element) >= 0 && ARRAY_INDEX(arr, element) < ARRAY_SIZE(arr))
#define ARRAY_COPY(dest, src) memcpy((dest), (src), sizeof(dest))
#define ARRAY_SET(arr, value) for (size_t i = 0; i < ARRAY_SIZE(arr); i++) { (arr)[i] = (value); }
#define ARRAY_FILL(arr, value) for (size_t i = 0; i < ARRAY_SIZE(arr); i++) { (arr)[i] = (value); }
#define ARRAY_SWAP(arr, i, j) { typeof((arr)[0]) temp = (arr)[i]; (arr)[i] = (arr)[j]; (arr)[j] = temp; }
#define ARRAY_REVERSE(arr) for (size_t i = 0; i < ARRAY_SIZE(arr) / 2; i++) { SWAP((arr)[i], (arr)[ARRAY_SIZE(arr) - 1 - i]); }


#define ARRAY_FIND(arr, value, index) do { \
    (index) = -1; \
    for (size_t i = 0; i < ARRAY_SIZE(arr); i++) { \
        if ((arr)[i] == (value)) { \
            (index) = i; \
            break; \
        } \
    } \
} while(0)




#define CIRCULAR_BUFF_SIZE 32
uint8_t circular_buff_idx = 0;
uint32_t circular_buff[CIRCULAR_BUFF_SIZE] = {0};


void circular_buff_add(uint32_t value) {
    circular_buff[circular_buff_idx] = value;
    circular_buff_idx += 1;

    if (circular_buff_idx >= CIRCULAR_BUFF_SIZE) {
        circular_buff_idx = 0;
    }
}   
