#ifndef FILE_STREAM_HPP
#define FILE_STREAM_HPP
#include <fstream>
#include "Stream.hpp"

namespace Diggler {

class InFileStream : public InStream, public SeekableStream {
protected:
	std::ifstream strm;

public:
	InFileStream(const std::string &path);
	~InFileStream();

	void readData(void *data, SizeT len) override;
	PosT tell() override;
	void seek(OffT, Whence = Set) override;
};

class OutFileStream : public OutStream, public SeekableStream {
protected:
	std::ofstream strm;

public:
	OutFileStream(const std::string &path);
	~OutFileStream();

	void writeData(const void *data, SizeT len) override;
	PosT tell() override;
	void seek(OffT, Whence = Set) override;
};

}

#endif
