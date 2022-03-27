#define FRACTION (1<<14)

int int_add_float(int i, int f);
int int_sub_float(int i, int f);
int float_add_int(int f, int i);
int float_sub_int(int f, int i);

int float_add_float(int f1, int f2);
int float_sub_float(int f1, int f2);

int int_mul_float(int i, int f);    
int float_mul_float(int f1, int f2);
int float_div_float(int f1, int f2);
int float_div_int(int f, int i);


int int_add_float(int i, int f){
    // returning float;
    return i*FRACTION + f;
}

int float_add_int(int f, int i){
    // returning float
    return f + i*FRACTION;
}

int int_sub_float(int i, int f){
    
    return i*FRACTION - f;
}

int float_sub_int(int f, int i){
    return f - i*FRACTION;
}

int float_add_float(int f1, int f2){
    return f1+f2;
}

int float_sub_float(int f1, int f2){
    return f1 - f2;
}

int int_mul_float(int i, int f){
    return i*f;
}

int float_mul_float(int f1, int f2){
    // maybe overflow 
    int64_t temp = f1;
    temp = temp * f2/FRACTION;
    return (int)temp;
}

int float_div_float(int f1, int f2) {
  int64_t temp = f1;
  temp = temp * FRACTION / f2;
  return (int)temp;
}

int float_div_int(int f, int i){
    return f/i;
}