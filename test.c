struct MyStruct;
struct Inner {
	void *inner1;
	struct MyStruct *inner2;
  char nameBuf[10];
};
struct MyStruct {
	int firstField;
	struct Inner flexibleArr[];
};

//typedef struct MyStruct MyStruct;

int my_fun(struct MyStruct *arr, int n) {
	int secret_int = 0;
  for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++) {
      char tmp = arr[0].flexibleArr[i].nameBuf[j];
      char tmp2 = arr[0].flexibleArr[i].inner2->firstField;
      secret_int += tmp + tmp2 + arr[i].firstField;
    }
	return secret_int;
}
