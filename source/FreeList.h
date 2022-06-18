#ifndef FREELIST_H
#define FREELIST_H
#include <vector>
using fl_handle = uint32_t;
template< typename T >
class FreeList
{
public:
	FreeList();
	fl_handle insert(const T& element) {

	}
private:
	struct Element {
		T data;
		int next = -1;
	};
	std::vector<Element> list;
	int first_free = -1;
	int elements = 0;
	int greatest_index = -1;
};

#endif // !FREELIST_H