#include "mctest.h"
#include <string.h>
#include <vector>

struct Entry { const char *name; McTestFn fn; };

static std::vector<Entry> &Registry(void)
{
	static std::vector<Entry> r;
	return r;
}

void McTestRegister(const char *name, McTestFn fn)
{
	Entry e = { name, fn };
	Registry().push_back(e);
}

int main(int argc, char **argv)
{
	const char *filter = argc > 1 ? argv[1] : nullptr;
	int ran = 0;
	for(size_t i = 0; i < Registry().size(); i++){
		const Entry &e = Registry()[i];
		if(filter && !strstr(e.name, filter))
			continue;
		printf("[ RUN ] %s\n", e.name);
		e.fn();
		printf("[ OK  ] %s\n", e.name);
		ran++;
	}
	if(ran == 0){
		fprintf(stderr, "no tests matched\n");
		return 1;
	}
	printf("%d test(s) passed\n", ran);
	return 0;
}
