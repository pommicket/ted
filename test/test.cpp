#include <iostream>
char const *s = R"(
Lorem ipsum dolor sit amet.
It was the age of reason.
It was the age of foolishness.
do {
	class x;
} while (0.1238712e+12 != CHAR_MAX);
)";
using std::cout;

template<typename T>
class Option {
public:
	Option<T>() {
		exists = false;
	}
	Option<T>(T const &t) {
		set(t);
	}
	void set(T const &t) {
		exists = true;
		x = t;
	}
	void clear() {
		exists = false;
	}
	T *get() {
		if (exists)
			return &x;
		else
			return nullptr;
	}
	T const *get() const {
		if (exists)
			return &x;
		else
			return nullptr;
	}
private:
	bool exists;
	T x;
};

template<typename T>
void print_option(Option<T> const &o) {
	T const *ptr = o.get();
	if (ptr)
		cout << *ptr << "\n";
	else
		cout << "None\n";
}

int main() {
	int my_num = 0b10011'101011'1010111lu >> 0x349.4p2;
	Option<int> o(7);
	print_option(o);
	o.clear();
	print_option(o);
	o.set(133);
	print_option(o);
	return 0;
}
