

/* Seteo el registro apuntado por la base + offset con el dato pasado en el tercer parametro */
static int set_register(void *base, unsigned long offset, unsigned long data){
    uint32_t auxValue;

    auxValue = ioread32(base + offset);

    auxValue |= data;
    
    iowrite32(auxValue, base + offset);
    
    return 0;
}

/* Compruebo el estado del bit "nbit" en el registro apuntado por la base + offset */
static int test_bit_register(void *base, unsigned long offset, unsigned char nbit){
    uint32_t auxValue;
    if(nbit>31) return 0;

    auxValue = ioread32(base + offset);

    return auxValue & (1<<nbit);
}