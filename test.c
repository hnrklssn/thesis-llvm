struct MyStruct {
	int firstField;
	void *secondField;
};

typedef struct MyStruct MyStruct;

int my_fun(MyStruct s) {
	return s.firstField;
}
