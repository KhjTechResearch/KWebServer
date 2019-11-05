#include "IStreamStdWrapper.h"
UnbufferedOLEStreamBuf::UnbufferedOLEStreamBuf(IStream* stream, std::ios_base::openmode which)
	: std::streambuf(), stream_(stream), readOnly_(!(which& std::ios_base::out)) {}

UnbufferedOLEStreamBuf::~UnbufferedOLEStreamBuf() { if (!readOnly_) UnbufferedOLEStreamBuf::sync(); stream_->Release(); }

bool UnbufferedOLEStreamBuf::backup() {
	LARGE_INTEGER liMove;
	liMove.QuadPart = -1LL;
	return SUCCEEDED(stream_->Seek(liMove, STREAM_SEEK_CUR, NULL));
}

int UnbufferedOLEStreamBuf::overflow(int ch) {
	if (ch != traits_type::eof()) {
		if (SUCCEEDED(stream_->Write(&ch, 1, NULL))) {
			return ch;
		}
	}
	return traits_type::eof();
}

int UnbufferedOLEStreamBuf::underflow() {
	char ch = UnbufferedOLEStreamBuf::uflow();
	if (ch != traits_type::eof()) {
		ch = backup() ? ch : traits_type::eof();
	}
	return ch;
}

int UnbufferedOLEStreamBuf::uflow() {
	char ch;
	ULONG cbRead;
	// S_FALSE indicates we've reached end of stream
	return (S_OK == stream_->Read(&ch, 1, &cbRead))
		? ch : traits_type::eof();
}

int UnbufferedOLEStreamBuf::pbackfail(int ch) {
	if (ch != traits_type::eof()) {
		ch = backup() ? ch : traits_type::eof();
	}
	return ch;
}

int UnbufferedOLEStreamBuf::sync() {
	return SUCCEEDED(stream_->Commit(STGC_DEFAULT)) ? 0 : -1;
}

std::ios::streampos UnbufferedOLEStreamBuf::seekpos(std::ios::streampos sp, std::ios_base::openmode which) {
	LARGE_INTEGER liMove;
	liMove.QuadPart = sp;
	return SUCCEEDED(stream_->Seek(liMove, STREAM_SEEK_SET, NULL)) ? sp : std::streampos(-1);
}

std::streampos UnbufferedOLEStreamBuf::seekoff(std::streamoff off, std::ios_base::seekdir way, std::ios_base::openmode which) {
	STREAM_SEEK sk;
	switch (way) {
	case std::ios_base::beg: sk = STREAM_SEEK_SET; break;
	case std::ios_base::cur: sk = STREAM_SEEK_CUR; break;
	case std::ios_base::end: sk = STREAM_SEEK_END; break;
	default: return -1;
	}
	LARGE_INTEGER liMove;
	liMove.QuadPart = static_cast<LONGLONG>(off);
	ULARGE_INTEGER uliNewPos;
	return SUCCEEDED(stream_->Seek(liMove, sk, &uliNewPos))
		? static_cast<std::streampos>(uliNewPos.QuadPart) : std::streampos (-1);
}

std::streamsize UnbufferedOLEStreamBuf::xsgetn(char* s, std::streamsize n) {
	ULONG cbRead;
	return SUCCEEDED(stream_->Read(s, static_cast<ULONG>(n), &cbRead))
		? static_cast<std::streamsize>(cbRead) : 0;
}

std::streamsize UnbufferedOLEStreamBuf::xsputn(const char* s, std::streamsize n) {
	ULONG cbWritten;
	return SUCCEEDED(stream_->Write(s, static_cast<ULONG>(n), &cbWritten))
		? static_cast<std::streamsize>(cbWritten) : 0;
}

std::streamsize UnbufferedOLEStreamBuf::showmanyc() {
	STATSTG stat;
	if (SUCCEEDED(stream_->Stat(&stat, STATFLAG_NONAME))) {
		std::streampos lastPos = static_cast<std::streampos>(stat.cbSize.QuadPart - 1);
		LARGE_INTEGER liMove;
		liMove.QuadPart = 0LL;
		ULARGE_INTEGER uliNewPos;
		if (SUCCEEDED(stream_->Seek(liMove, STREAM_SEEK_CUR, &uliNewPos))) {
			
			return std::max<std::streamsize>(lastPos - static_cast<std::streampos>(uliNewPos.QuadPart), 0);
		}
	}
	return 0;
}

std::streambuf* StdStreamBufFromOLEStream(IStream* pStream, std::ios_base::openmode which)
{
	return new (std::nothrow) UnbufferedOLEStreamBuf(pStream, which);
}

