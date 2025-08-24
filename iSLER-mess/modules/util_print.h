#define PRINT_SEPARATOR() printf("\n-----------------------------------\n")

// format example: "%02X", "%04X", "%08X", "%u"
#define PRINT_ARRAY(arr, format) do { \
    for (size_t i = 0; i < ARRAY_SIZE(arr); i++) { \
        printf(format " ", arr[i]); \
    } \
    printf("\n"); \
} while(0)


#define PRINT_ARRAY_WITH_SIZE(arr, size, format) do { \
    for (size_t i = 0; i < size; i++) { \
        printf(format " ", arr[i]); \
    } \
    printf("\n"); \
} while(0)


#define PRINT_STRUCT_BYTES(struct_ptr, format) do { \
    const uint8_t* bytes = (const uint8_t*)(struct_ptr); \
    for (size_t i = 0; i < sizeof(*(struct_ptr)); i++) { \
        printf(format " ", bytes[i]); \
    } \
    printf("\n"); \
} while(0)