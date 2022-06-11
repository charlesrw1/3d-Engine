#ifndef FILES_H
#define FILES_H
#include <vector>
#include <cassert>
using u8 = unsigned char;
using u32 = unsigned int;
class BinaryFile
{
public:
	template< typename T >
	T read() {
		T res;
		assert(buffer.size() > index_ptr + sizeof(T));
		memcpy(&res, &buffer[index_ptr], sizeof(T));
		index_ptr += sizeof(T);
		return res;
	}
	template< typename T >
	void write(T& data) {
		buffer.resize(index_ptr + sizeof(T) + 1);
		memcpy(&buffer[index_ptr], &data, sizeof(T));
		index_ptr += sizeof(T);
	}
	void write(const char* string) {
		int len = strlen(string);
		buffer.resize(index_ptr + len + 2);
		memcpy(&buffer[index_ptr], string, len + 1);
		index_ptr += len + 1;
	}
	void advance(u32 bytes) {
		index_ptr += bytes;
	}
	void seek(u32 pos) {
		assert(pos <= buffer.size());
		index_ptr = pos;
	}
	u32 get_ptr() { return index_ptr; }
	void write_to_disk(const char* file_name);
	void read_from_disk(const char* file_name);

private:
	std::vector<u8> buffer;
	u32 index_ptr{};
};

class TextFile
{

};


#endif // !FILES_H

