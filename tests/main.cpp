#include "check.hpp"
int main() {
	for (auto& t : tests()) {
		int before = failures();
		t.fn();
		std::printf("%s %s\n", failures() == before ? "ok  " : "FAIL", t.name);
	}
	std::printf("\n%zu tests, %d failed checks\n", tests().size(), failures());
	return failures() ? 1 : 0;
}
