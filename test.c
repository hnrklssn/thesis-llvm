struct MyStruct;
struct Inner {
	void *inner1;
	struct MyStruct *inner2;
};
struct MyStruct {
	int firstField;
	struct Inner secondField;
};

typedef struct MyStruct MyStruct;

int my_fun(MyStruct s, int ***arr, int n) {
	int secret_int;
	if(s.secondField.inner1) {
		secret_int = s.secondField.inner2->firstField;
	} else {
		secret_int = 0;
	}
  for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++)
      secret_int += arr[i][j][5];
	return s.firstField + secret_int;
}
