#pragma once
#include <ios>
#include <streambuf>
#include <windows.h>
#include <algorithm>
// Read-write unbuffered streambuf implementation which uses no
// internal buffers (conventional wisdom says this can't be done
// except for write-only streams, but I adapted Matt Austern's example
// from http://www.drdobbs.com/184401305).

class UnbufferedOLEStreamBuf : public std::streambuf {
public:
	UnbufferedOLEStreamBuf(IStream* stream, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out);
	~UnbufferedOLEStreamBuf();

protected:
	virtual int overflow(int ch = traits_type::eof());
	virtual int underflow();
	virtual int uflow();
	virtual int pbackfail(int ch = traits_type::eof());
	virtual int sync();
	virtual std::streampos seekpos(std::streampos sp, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out);
	virtual std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out);
	virtual std::streamsize xsgetn(char* s, std::streamsize n);
	virtual std::streamsize xsputn(const char* s, std::streamsize n);
	virtual std::streamsize showmanyc();

private:
	IStream* stream_;
	bool readOnly_;
	bool backup();
};

