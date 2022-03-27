#include <stdio.h>
#include <syscall.h>

int char_to_int(char *str){
    int i=0, len, result=0, temp, j;
    for(;*(str+i)!='\0';i++);
    len = i;

    for(i=0;i<len;i++){
        temp = 1;
        for(j=0;j<len-i-1;j++)
            temp = temp *10;
        result += (*(str+i) - '0')*temp;
    }
    return result;
}

int
main (int argc, char **argv)
{
  if(argc!=5){
      printf("argc : %d\n", argc);
      printf("argc must be 5.\n");
      return 0;
  }

  int array[5], i, a, b;
  for(i=1;i<5;i++){
    array[i] = char_to_int(argv[i]);
  }
  a = fibonacci(array[1]);
  b = max_of_four_int(array[1], array[2], array[3], array[4]);
  printf("%d %d\n", a, b);

  return EXIT_SUCCESS;
}
