

#ifdef _WIN32
#if _WIN32_WINNT < 0x0601
#undef  _WIN32_WINNT
#define _WIN32_WINNT 0x0601 
#endif






#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>




extern "C" {
	typedef bool(*fun1_t)(LOGICAL_PROCESSOR_RELATIONSHIP,
		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, PDWORD);
	typedef bool(*fun2_t)(USHORT, PGROUP_AFFINITY);
	typedef bool(*fun3_t)(HANDLE, CONST GROUP_AFFINITY*, PGROUP_AFFINITY);
}

#endif

#include <fstream>
#include <iomanip>

#include <sstream>


#include <ctime>	
#include <cstring>	
#include <cmath>	
#include <cstdio>	

#if defined(__linux__) && !defined(__ANDROID__)
#include <stdlib.h>
#include <sys/mman.h> 
#endif

#include "misc.h"
#include "thread.h"
#include "usi.h"

using namespace std;

namespace {

	
	
	

	
	
	
	struct Tie : public streambuf
	{
		Tie(streambuf* buf_, streambuf* log_) : buf(buf_), log(log_) {}

		int sync() override { return log->pubsync(), buf->pubsync(); }
		int overflow(int c) override { return write(buf->sputc((char)c), "<< "); }
		int underflow() override { return buf->sgetc(); }
		int uflow() override { return write(buf->sbumpc(), ">> "); }

		int write(int c, const char* prefix) {
			static int last = '\n';
			if (last == '\n')
				log->sputn(prefix, 3);
			return last = log->sputc((char)c);
		}

		streambuf *buf, *log; 
	};

	struct Logger {
		static void start(bool b)
		{
			static Logger log;

			if (b && !log.file.is_open())
			{
				log.file.open("io_log.txt", ifstream::out);
				cin.rdbuf(&log.in);
				cout.rdbuf(&log.out);
				cout << "start logger" << endl;
			}
			else if (!b && log.file.is_open())
			{
				cout << "end logger" << endl;
				cout.rdbuf(log.out.buf);
				cin.rdbuf(log.in.buf);
				log.file.close();
			}
		}

	private:
		Tie in, out;   
		ofstream file; 

		
#pragma warning (disable : 4068) 
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
		Logger() : in(cin.rdbuf(), file.rdbuf()), out(cout.rdbuf(), file.rdbuf()) {}
#pragma clang diagnostic pop

		~Logger() { start(false); }
	};

} 


void start_logger(bool b) { Logger::start(b); }





const string engine_info() {

	stringstream ss;

	
	ifstream ifs("engine_name.txt");
	if (!ifs.fail())
	{
		
		string str = "default engine";
		Tools::getline(ifs, str);
		ss << "id name " << str << endl;

		
		str = "default author";
		Tools::getline(ifs, str);
		ss << "id author " << str << endl;
	}
	else
	{
		ss << "id name " <<
			
#if defined(ENGINE_NAME_FROM_MAKEFILE)
			
			
#define STRINGIFY(n) #n
#define TOSTRING(n) STRINGIFY(n)
			TOSTRING(ENGINE_NAME_FROM_MAKEFILE)
#undef STRINGIFY
#undef TOSTRING
#else
			ENGINE_NAME
#endif
			<< ' '
#if !defined(USER_ENGINE)
			<< EVAL_TYPE_NAME << ' '
#endif
			<< ENGINE_VERSION << std::setfill('0')
			<< (Is64Bit ? " 64" : " 32")
			<< TARGET_CPU
#if defined(FOR_TOURNAMENT)
			<< " TOURNAMENT"
#endif

#if defined(EVAL_LEARN)
			<< " EVAL_LEARN"
#endif

			<< endl
#if defined(USER_ENGINE)
			<< "id author by yaneurao , tayayan" << std::endl;
#elif !defined(YANEURAOU_ENGINE_DEEP)
			<< "id author by yaneurao" << std::endl;
#else
			<< "id author by Tadao Yamaoka , yaneurao" << std::endl;
#endif
	}

	return ss.str();
}


const std::string compiler_info() {

#define stringify2(x) #x
#define stringify(x) stringify2(x)
#define make_version_string(major, minor, patch) stringify(major) "." stringify(minor) "." stringify(patch)

	
	
	
	
	
	
	

	std::string compiler = "\nCompiled by ";

#ifdef __clang__
	compiler += "clang++ ";
	compiler += make_version_string(__clang_major__, __clang_minor__, __clang_patchlevel__);
#elif __INTEL_COMPILER
	compiler += "Intel compiler ";
	compiler += "(version ";
	compiler += stringify(__INTEL_COMPILER) " update " stringify(__INTEL_COMPILER_UPDATE);
	compiler += ")";
#elif _MSC_VER
	compiler += "MSVC ";
	compiler += "(version ";
	compiler += stringify(_MSC_FULL_VER) "." stringify(_MSC_BUILD);
	compiler += ")";
#elif __GNUC__
	compiler += "g++ (GNUC) ";
	compiler += make_version_string(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
	compiler += "Unknown compiler ";
	compiler += "(unknown version)";
#endif

#if defined(__APPLE__)
	compiler += " on Apple";
#elif defined(__CYGWIN__)
	compiler += " on Cygwin";
#elif defined(__MINGW64__)
	compiler += " on MinGW64";
#elif defined(__MINGW32__)
	compiler += " on MinGW32";
#elif defined(__ANDROID__)
	compiler += " on Android";
#elif defined(__linux__)
	compiler += " on Linux";
#elif defined(_WIN64)
	compiler += " on Microsoft Windows 64-bit";
#elif defined(_WIN32)
	compiler += " on Microsoft Windows 32-bit";
#else
	compiler += " on unknown system";
#endif

#ifdef __VERSION__
	
	compiler += "\n __VERSION__ macro expands to: ";
	compiler += __VERSION__;
#else
	compiler += "(undefined macro)";
#endif

	compiler += "\n";

	return compiler;
}





static std::atomic<int64_t> hits[2], means[2];

void dbg_hit_on(bool b) { ++hits[0]; if (b) ++hits[1]; }
void dbg_hit_on(bool c, bool b) { if (c) dbg_hit_on(b); }
void dbg_mean_of(int v) { ++means[0]; means[1] += v; }

void dbg_print() {

	if (hits[0])
		cerr << "Total " << hits[0] << " Hits " << hits[1]
		<< " hit rate (%) " << fixed << setprecision(3) << (100.0f * hits[1] / hits[0]) << endl;

	if (means[0])
		cerr << "Total " << means[0] << " Mean "
		<< (double)means[1] / means[0] << endl;
}





std::ostream& operator<<(std::ostream& os, SyncCout sc) {

	static std::mutex m;

	if (sc == IO_LOCK)
		m.lock();

	if (sc == IO_UNLOCK)
		m.unlock();

	return os;
}






#if defined (NO_PREFETCH)

void prefetch(void*) {}

#else

void prefetch(void* addr) {

	
#if defined (USE_SSE2)

	
	
	ASSERT_LV3(((u64)addr & 0x1f) == 0);

#  if defined(__INTEL_COMPILER)
	
	__asm__("");
#  endif

	
	
	

#  if defined(__INTEL_COMPILER) || defined(_MSC_VER)
	_mm_prefetch((char*)addr, _MM_HINT_T0);
	
#  else
	__builtin_prefetch(addr);
#  endif

#endif
}

#endif





namespace {
	
	bool largeMemoryAllocFirstCall = true;
}




#if defined(__linux__) && !defined(__ANDROID__)

void* aligned_ttmem_alloc(size_t allocSize, void*& mem , size_t align  ) {

	constexpr size_t alignment = 2 * 1024 * 1024; 
	size_t size = ((allocSize + alignment - 1) / alignment) * alignment; 
	if (posix_memalign(&mem, alignment, size))
		mem = nullptr;
	madvise(mem, allocSize, MADV_HUGEPAGE);

	
	if (largeMemoryAllocFirstCall)
	{
		sync_cout << "info string Hash table allocation: Linux Large Pages used." << sync_endl;
		largeMemoryAllocFirstCall = false;
	}

	return mem;
}

#elif defined(_WIN64)

static void* aligned_ttmem_alloc_large_pages(size_t allocSize) {

	
#if defined(USER_ENGINE)
	
	return nullptr;
#else
	if (!Options["LargePageEnable"])
		return nullptr;
#endif

	HANDLE hProcessToken{ };
	LUID luid{ };
	void* mem = nullptr;

	const size_t largePageSize = GetLargePageMinimum();

	
	

	if (!largePageSize)
		return nullptr;

	
	

	
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hProcessToken))
		return nullptr;

	if (LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &luid))
	{
		TOKEN_PRIVILEGES tp{ };
		TOKEN_PRIVILEGES prevTp{ };
		DWORD prevTpLen = 0;

		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = luid;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		
		
		if (AdjustTokenPrivileges(
			hProcessToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), &prevTp, &prevTpLen) &&
			GetLastError() == ERROR_SUCCESS)
		{
			
			allocSize = (allocSize + largePageSize - 1) & ~size_t(largePageSize - 1);
			mem = VirtualAlloc(
				NULL, allocSize, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);

			
			AdjustTokenPrivileges(hProcessToken, FALSE, &prevTp, 0, NULL, NULL);
		}
	}

	CloseHandle(hProcessToken);

	return mem;
}

void* aligned_ttmem_alloc(size_t allocSize , void*& mem , size_t align ) {

	

	
	mem = aligned_ttmem_alloc_large_pages(allocSize);

	
	

	
	
	

	
	


	if (largeMemoryAllocFirstCall)
	{
		if (mem)
			sync_cout << "info string Hash table allocation: Windows Large Pages used." << sync_endl;
		else
			sync_cout << "info string Hash table allocation: Windows Large Pages not used." << sync_endl;

		largeMemoryAllocFirstCall = false;
	}

	
	
	if (!mem)
		mem = VirtualAlloc(NULL, allocSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	

	

	return mem;
}

#else

void* aligned_ttmem_alloc(size_t allocSize, void*& mem , size_t align) {

	

	
	const size_t alignment = align;

	size_t size = allocSize + alignment - 1; 
	mem = malloc(size);

	if (largeMemoryAllocFirstCall)
	{
		sync_cout << "info string Hash table allocation: Large Pages not used." << sync_endl;
		largeMemoryAllocFirstCall = false;
	}

	void* ret = reinterpret_cast<void*>((uintptr_t(mem) + alignment - 1) & ~uintptr_t(alignment - 1));
	return ret;
}

#endif


#if defined(_WIN64)

void aligned_ttmem_free(void* mem) {

	if (mem && !VirtualFree(mem, 0, MEM_RELEASE))
	{
		DWORD err = GetLastError();
		std::cerr << "Failed to free transposition table. Error code: 0x" <<
			std::hex << err << std::dec << std::endl;
		Tools::exit();
	}
}

#else

void aligned_ttmem_free(void* mem) {
	free(mem);
}

#endif






void* LargeMemory::alloc(size_t size, size_t align , bool zero_clear)
{
	free();
	return static_alloc(size, this->mem, align, zero_clear);
}



void LargeMemory::free()
{
	static_free(mem);
	mem = nullptr;
}


void* LargeMemory::static_alloc(size_t size, void*& mem, size_t align, bool zero_clear)
{
	void* ptr = aligned_ttmem_alloc(size, mem, align);

	auto error_exit = [&](std::string mes) {
		sync_cout << "info string Error! : " << mes << " in LargeMemory::alloc(" << size << "," << align << ")" << sync_endl;
		Tools::exit();
	};

	
	if (ptr == nullptr)
		error_exit("can't alloc enough memory.");

	
	if ((reinterpret_cast<size_t>(ptr) % align) != 0)
		error_exit("can't alloc algined memory.");

	
	if (zero_clear)
	{
		
		if (size < 256 * 1024 * 1024)
			
			memset(ptr, 0, size);
		else
			
			Tools::memclear(nullptr, ptr, size);
	}

	return ptr;
}


void LargeMemory::static_free(void* mem)
{
	aligned_ttmem_free(mem);
}







namespace WinProcGroup {

#if !defined ( _WIN32 )

	void bindThisThread(size_t) {}

#else


	
	
	

	int best_group(size_t idx) {

		
		
		

		
		int threads = 0;

		
		int nodes = 0;

		
		int cores = 0;

		DWORD returnLength = 0;
		DWORD byteOffset = 0;

		
		HMODULE k32 = GetModuleHandle(L"Kernel32.dll");
		auto fun1 = (fun1_t)(void(*)())GetProcAddress(k32, "GetLogicalProcessorInformationEx");
		if (!fun1)
			return -1;

		
		if (fun1(RelationAll, nullptr, &returnLength))
			return -1;

		
		SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *buffer, *ptr;
		ptr = buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)malloc(returnLength);

		
		if (!fun1(RelationAll, buffer, &returnLength))
		{
			free(buffer);
			return -1;
		}

		while (byteOffset < returnLength)
		{
			
			if (ptr->Relationship == RelationNumaNode)
				nodes++;

			else if (ptr->Relationship == RelationProcessorCore)
			{
				
				cores++;

				
				threads += (ptr->Processor.Flags == LTP_PC_SMT) ? 2 : 1;
			}

			ASSERT_LV3(ptr->Size);
			byteOffset += ptr->Size;
			ptr = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)(((char*)ptr) + ptr->Size);
		}

		free(buffer);

		std::vector<int> groups;

		
		
		for (int n = 0; n < nodes; n++)
			for (int i = 0; i < cores / nodes; i++)
				groups.push_back(n);

		
		
		

		
		

		for (int t = 0; t < threads - cores; t++)
			groups.push_back(t % nodes);

		
		
		return idx < groups.size() ? groups[idx] : -1;

		
		
		
	}

	

	void bindThisThread(size_t idx) {

#if defined(_WIN32) && !defined(USER_ENGINE)
		idx += Options["ThreadIdOffset"];
#endif

		

		
		int group = best_group(idx);

		if (group == -1)
			return;

		
		HMODULE k32 = GetModuleHandle(L"Kernel32.dll");
		auto fun2 = (fun2_t)(void(*)())GetProcAddress(k32, "GetNumaNodeProcessorMaskEx");
		auto fun3 = (fun3_t)(void(*)())GetProcAddress(k32, "SetThreadGroupAffinity");

		if (!fun2 || !fun3)
			return;

		GROUP_AFFINITY affinity;
		if (fun2(group, &affinity))
			fun3(GetCurrentThread(), &affinity, nullptr);
	}

#endif

} 






TimePoint Timer::elapsed() const { return TimePoint(Search::Limits.npmsec ? Threads.nodes_searched() : now() - startTime); }
TimePoint Timer::elapsed_from_ponderhit() const { return TimePoint(Search::Limits.npmsec ? Threads.nodes_searched() : now() - startTimeFromPonderhit); }

#if defined(USE_TIME_MANAGEMENT)



TimePoint Timer::round_up(TimePoint t0) const
{
	
	auto t = std::max(((t0 + 999) / 1000) * 1000, minimum_thinking_time);

	
	t = t - network_delay;

	
	if (t < t0)
		t += 1000;

	
	t = std::min(t, remain_time);
	return t;
}

#endif

Timer Time;








namespace Tools
{
	

	
	
	void memclear(const char* name_, void* table, size_t size)
	{
		
		
		
		

		

		
		auto thread_num = (size_t)Threads.size(); 

		if (name_ != nullptr)
			sync_cout << "info string " + std::string(name_) + " : Start clearing with " <<  thread_num << " threads , Hash size =  " << size / (1024 * 1024) << "[MB]" << sync_endl;

		

		std::vector<std::thread> threads;

		for (size_t idx = 0; idx < thread_num; idx++)
		{
			threads.push_back(std::thread([table, size, thread_num, idx]() {

				

				
				if (thread_num > 8)
					WinProcGroup::bindThisThread(idx);

				
				const size_t stride = size / thread_num,
					start = stride * idx,
					len = idx != thread_num - 1 ?
					stride : size - start;

				std::memset((uint8_t*)table + start, 0, len);
				}));
		}

		for (std::thread& th : threads)
			th.join();

		if (name_ != nullptr)
			sync_cout << "info string " + std::string(name_) + " : Finish clearing." << sync_endl;
	}

	
	
	void exit()
	{
		sleep(3000); 
		::exit(EXIT_FAILURE);
	}

	
	void sleep(u64 ms)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}

	
	std::string now_string()
	{
		
		

#if defined(_MSC_VER)
		
#pragma warning(disable : 4996)
#endif

		auto now = std::chrono::system_clock::now();
		auto tp = std::chrono::system_clock::to_time_t(now);
		auto result = string(std::ctime(&tp));

		
		while (*result.rbegin() == '\n' || (*result.rbegin() == '\r'))
			result.pop_back();
		return result;
	}

	
	
	
	
	bool getline(std::ifstream& fs, std::string& s)
	{
		bool b = (bool)std::getline(fs, s);
		StringExtension::trim_inplace(s);
		return b;
	}

	
	
	std::wstring MultiByteToWideChar(const std::string& s)
	{
#if !defined(_WIN32)
		return std::wstring(s.begin(), s.end()); 
		
		
#else

		
		

		

		int length = ::MultiByteToWideChar(
			CP_THREAD_ACP,			
			MB_PRECOMPOSED,			
			s.c_str(),				
			(int)s.length() + 1,	
			nullptr,				
			0						
		);

		
		

		
		

		
		

		wchar_t* buffer = new wchar_t[length];
		SCOPE_EXIT( delete[] buffer; );

		int result = ::MultiByteToWideChar(
			CP_THREAD_ACP,			
			MB_PRECOMPOSED,			
			s.c_str(),				
			(int)s.length() + 1,	
			buffer,					
			length					
		);

		if (result == 0)
			return std::wstring(); 

		return std::wstring(buffer);
#endif
	}

	
	std::string to_string(ResultCode code)
	{
		

		switch (code)
		{
		case ResultCode::Ok                   : return "Ok";
		case ResultCode::MemoryAllocationError: return "MemoryAllocationError";
		case ResultCode::SomeError            : return "SomeError";
		case ResultCode::FileOpenError        : return "FileOpenError";
		case ResultCode::FileReadError        : return "FileReadError";
		case ResultCode::FileWriteError       : return "FileWriteError";
		case ResultCode::CreateFolderError    : return "CreateFolderError";
		case ResultCode::NotImplementedError  : return "NotImplementedError";
		default                               : return "OtherError";
		}
	}
}





namespace SystemIO
{

	
	
	Tools::Result ReadAllLines(const std::string& filename, std::vector<std::string>& lines, bool trim)
	{
#if 0
		ifstream fs(filename);
		if (fs.fail())
			return 1; 

		while (!fs.fail() && !fs.eof())
		{
			std::string line;
			Dependency::getline(fs, line);
			if (trim)
				line = StringExtension::trim(line);
			if (line.length())
				lines.push_back(line);
		}
		fs.close();

		return 0;
#endif

		
		

		TextReader reader;

		
		reader.SetTrim(trim);
		
		reader.SkipEmptyLine(true);

		auto result = reader.Open(filename);
		if (!result.is_ok())
			return result;

		string line;
		while (reader.ReadLine(line).is_ok())
			lines.emplace_back(line);

		return Tools::Result::Ok();
	}

	Tools::Result ReadFileToMemory(const std::string& filename, std::function<void* (size_t)> callback_func)
	{
		

		FILE* fp = fopen(filename.c_str(), "rb");
		if (fp == nullptr)
			return Tools::Result(Tools::ResultCode::FileOpenError);

		fseek(fp, 0, SEEK_END);
		size_t endPos = (size_t)ftell(fp);
		fseek(fp, 0, SEEK_SET);
		size_t beginPos = (size_t)ftell(fp);
		size_t file_size = endPos - beginPos;

		
		
		void* ptr = callback_func(file_size);

		
		
		
		if (ptr == nullptr)
			return Tools::Result(Tools::ResultCode::MemoryAllocationError);

		

		const size_t block_size = 1024 * 1024 * 1024; 
		for (size_t pos = 0; pos < file_size; pos += block_size)
		{
			
			size_t read_size = (pos + block_size < file_size) ? block_size : (file_size - pos);

			if (fread((u8*)ptr + pos, 1, read_size, fp) != read_size)
				
				return Tools::Result(Tools::ResultCode::FileReadError);

			
		}
		fclose(fp);

		return Tools::Result::Ok();
	}


	Tools::Result WriteMemoryToFile(const std::string& filename, void* ptr, size_t size)
	{
		fstream fs(filename, ios::out | ios::binary);
		if (fs.fail())
			return Tools::Result(Tools::ResultCode::FileOpenError);

		const u64 block_size = 1024 * 1024 * 1024; 
		for (u64 pos = 0; pos < size; pos += block_size)
		{
			
			u64 write_size = (pos + block_size < size) ? block_size : (size - pos);
			fs.write((char*)ptr + pos, write_size);
			

			if (fs.fail())
				return Tools::Result(Tools::ResultCode::FileWriteError); 
		}

		
		fs.close();

		return Tools::Result::Ok();
	}

	

	
	
	TextReader::TextReader()
	{
		buffer.resize(1024 * 1024);
		line_buffer.reserve(2048);
		clear();

		
		
		trim = false;
		skipEmptyLine = false;
	}

	TextReader::~TextReader()
	{
		Close();
	}

	
	void TextReader::clear()
	{
		fp = nullptr;
		is_eof = false;
		cursor = 0;
		read_size = 0;
		is_prev_cr = false;
	}

	
	Tools::Result TextReader::Open(const std::string& filename)
	{
		Close();

		
		fp = fopen(filename.c_str(), "rb");
		return (fp == nullptr) ? Tools::Result(Tools::ResultCode::FileOpenError) : Tools::Result::Ok();
	}

	
	void TextReader::Close()
	{
		if (fp != nullptr)
			fclose(fp);

		clear();
	}

	
	int TextReader::read_char()
	{
		
		while (!(is_eof && cursor >= read_size))
		{
			if (cursor < read_size)
				return (int)buffer[cursor++];

			
			read_next_block();
		}
		return -1;
	}

	
	
	Tools::Result TextReader::read_line_simple()
	{
		
		
		line_buffer.clear();

		

		while (true)
		{
			int c = read_char();
			if (c == -1 )
			{
				
				
				if (line_buffer.size() == 0)
					return Tools::ResultCode::Eof;

				break;
			}

			if (c == '\r')
			{
				
				is_prev_cr = true;
				break;
			}

			
			
			auto prev_cr = is_prev_cr;
			is_prev_cr = false;

			if (c == '\n')
			{
				if (!prev_cr)
					break;
				
				
			}
			else
			{
				
				line_buffer.push_back(c);
			}
		}

		
		

		return Tools::ResultCode::Ok;
	}


	
	Tools::Result TextReader::ReadLine(std::string& line)
	{
		while (true)
		{
			if (read_line_simple().is_eof())
				return Tools::ResultCode::Eof;

			
			if (trim)
				while (line_buffer.size() > 0)
				{
					char c = *line_buffer.rbegin();
					if (!(c == ' ' || c == '\t'))
						break;

					line_buffer.resize(line_buffer.size() - 1);
				}

			
			if (skipEmptyLine && line_buffer.size() == 0)
				continue;

			line = std::string((const char*)line_buffer.data(), line_buffer.size());
			return Tools::ResultCode::Ok;
		}
	}

	
	void TextReader::read_next_block()
	{
		if (::feof(fp))
			read_size = 0;
		else
			read_size = ::fread(&buffer[0], sizeof(u8), buffer.size(), fp);

		
		cursor = 0;

		
		is_eof = read_size == 0;
	}

	

	
	Tools::Result BinaryBase::close()
	{
		Tools::ResultCode result = Tools::ResultCode::Ok;
		if (fp != nullptr)
		{
			if (fclose(fp) != 0)
				result = Tools::ResultCode::FileCloseError;
			fp = nullptr;
		}
		return Tools::Result(result);
	}

	

	
	Tools::Result BinaryReader::open(const std::string& filename)
	{
		fp = fopen(filename.c_str(), "rb");
		if (fp == nullptr)
			return Tools::Result(Tools::ResultCode::FileOpenError);

		return Tools::Result::Ok();
	}

	
	
	size_t BinaryReader::get_size()
	{
		ASSERT_LV3(fp != nullptr);

		fseek(fp, 0, SEEK_END);
		size_t endPos = (size_t)ftell(fp);
		fseek(fp, 0, SEEK_SET);
		size_t beginPos = (size_t)ftell(fp);
		size_t file_size = endPos - beginPos;

		return file_size;
	}

	
	Tools::Result BinaryReader::read(void* ptr, size_t size)
	{
		if (fread((u8*)ptr, 1, size, fp) != size)
			return Tools::Result(Tools::ResultCode::FileReadError);

		return Tools::Result::Ok();
	}

	

	
	Tools::Result BinaryWriter::open(const std::string& filename)
	{
		fp = fopen(filename.c_str(), "wb");
		if (fp == nullptr)
			return Tools::Result(Tools::ResultCode::FileOpenError);

		return Tools::Result::Ok();
	}

	
	Tools::Result BinaryWriter::write(void* ptr, size_t size)
	{
		if (fwrite((u8*)ptr, 1, size, fp) != size)
			return Tools::Result(Tools::ResultCode::FileWriteError);

		return Tools::Result::Ok();
	}
}





namespace Parser
{

	

	
	std::string LineScanner::peek_text()
	{
		
		if (!token.empty())
			return token;

		

		while (!raw_eol())
		{
			char c = line[pos++];
			if (c == ' ')
				break;
			token += c;
		}
		return token;
	}

	
	std::string LineScanner::get_text()
	{
		auto result = (!token.empty() ? token : peek_text());
		token.clear();
		return result;
	}

	
	s64 LineScanner::get_number(s64 defaultValue)
	{
		std::string token = get_text();
		return token.empty() ? defaultValue : atoll(token.c_str());
	}

	
	double LineScanner::get_double(double defaultValue)
	{
		std::string token = get_text();
		return token.empty() ? defaultValue : atof(token.c_str());
	}

}





double Math::sigmoid(double x) {
	return 1.0 / (1.0 + std::exp(-x));
}

double Math::dsigmoid(double x) {
	return sigmoid(x) * (1.0 - sigmoid(x));
}








namespace Path
{
	
	
	
	std::string Combine(const std::string& folder, const std::string& filename)
	{
		if (folder.length() >= 1 && *folder.rbegin() != '/' && *folder.rbegin() != '\\')
			return folder + "/" + filename;

		return folder + filename;
	}

	
	std::string GetFileName(const std::string& path)
	{
		
		auto path_index1 = path.find_last_of("\\");
		auto path_index2 = path.find_last_of("/");

		
		
		if (path_index1 == std::string::npos && path_index2 == std::string::npos)
			return path;

		
		
		path_index1 = path_index1 == string::npos ? 0 : path_index1;
		path_index2 = path_index2 == string::npos ? 0 : path_index2;
		auto path_index = std::max(path_index1, path_index2);

		
		return path.substr(path_index + 1);
	}

	
	std::string GetDirectoryName(const std::string& path)
	{
		

		auto length = path.length() - GetFileName(path).length() - 1;
		return (length == 0) ? "" : path.substr(0,length);
	}

};





namespace {
	
	string to_upper(const string source)
	{
		std::string destination;
		destination.resize(source.size());
		std::transform(source.cbegin(), source.cend(), destination.begin(), [](char c) { return (char)toupper(c); });
		return destination;
	}
}

namespace StringExtension
{
	
	
	
	bool stricmp(const string& s1, const string& s2)
	{
		
		

		return to_upper(s1) != to_upper(s2);
	}

	
	bool is_space(char c) { return c == '\r' || c == '\n' || c == ' ' || c == '\t'; }

	
	bool is_number(char c) { return '0' <= c && c <= '9'; }

	
	std::string trim(const std::string& input)
	{
		
		string s = input;

		
		
		
		

		auto cur = s.length();

		
		
		while (cur > 0 && is_space(s[cur-1]))
			cur--;

		s.resize(cur);
		return s;
	}

	
	void trim_inplace(std::string& s)
	{
		auto cur = s.length();

		while (cur > 0 && is_space(s[cur-1]))
			cur--;

		s.resize(cur);
	}

	
	
	std::string trim_number(const std::string& input)
	{
		string s = input;
		auto cur = s.length();

		
		

		while (cur > 0 && is_space(s[cur-1]))
			cur--;

		while (cur > 0 && is_number(s[cur-1]))
			cur--;

		while (cur > 0 && is_space(s[cur-1]))
			cur--;

		s.resize(cur);
		return s;
	}

	
	void trim_number_inplace(std::string& s)
	{
		auto cur = s.length();

		while (cur > 0 && is_space(s[cur - 1]))
			cur--;

		while (cur > 0 && is_number(s[cur - 1]))
			cur--;

		while (cur > 0 && is_space(s[cur - 1]))
			cur--;

		s.resize(cur);
	}

	
	int to_int(const std::string input, int default_)
	{
		
		
		

		std::istringstream ss(input);
		int result = default_; 
		ss >> result;
		return result;
	}

	
	std::vector<std::string> split(const std::string& input)
	{
		auto result = std::vector<string>();
		Parser::LineScanner scanner(input);
		while (!scanner.eol())
			result.push_back(scanner.get_text());

		return result;
	}

	
	bool StartsWith(std::string const& value, std::string const& starting)
	{
		if (starting.size() > value.size()) return false;
		return std::equal(starting.begin(), starting.end(), value.begin());
	};

	
	bool EndsWith(std::string const& value, std::string const& ending)
	{
		if (ending.size() > value.size()) return false;
		return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
	};

};





#if defined(_MSC_VER)


#include <filesystem>

#elif defined(__GNUC__)




#include <dirent.h>
#endif



namespace Directory
{
	
	
	
	std::vector<std::string> EnumerateFiles(const std::string& sourceDirectory, const string& extension)
	{
		std::vector<std::string> filenames;

#if defined(_MSC_VER)
		
		

		namespace fs = std::filesystem;

		
		
		auto src = sourceDirectory.empty() ? fs::current_path() : fs::path(sourceDirectory);

		for (auto ent : fs::directory_iterator(src))
			if (fs::is_regular_file(ent)
				&& StringExtension::EndsWith(ent.path().filename().string(), extension))

				filenames.push_back(Path::Combine(ent.path().parent_path().string(), ent.path().filename().string()));

#elif defined(__GNUC__)

		
		DIR* dp;       
		dirent* entry; 

		dp = opendir(sourceDirectory.c_str());
		if (dp != NULL)
		{
			do {
				entry = readdir(dp);
				
				
				if (entry != NULL && StringExtension::EndsWith(entry->d_name, extension))
				{
					
					filenames.push_back(Path::Combine(sourceDirectory, entry->d_name));
				}
			} while (entry != NULL);
			closedir(dp);
		}
#endif

		return filenames;
	}

	
	
	
	std::string GetCurrentFolder() { return CommandLine::workingDirectory; }
}












#if defined(_WIN32)


#if defined(_MSC_VER)

namespace Directory {
	Tools::Result CreateFolder(const std::string& dir_name)
	{
		
		
		

		int result =  _wmkdir(Tools::MultiByteToWideChar(dir_name).c_str());
		

		return result == 0 ? Tools::Result::Ok() : Tools::Result(Tools::ResultCode::CreateFolderError);
	}
}

#elif defined(__GNUC__)

#include <direct.h>
namespace Directory {
	Tools::Result CreateFolder(const std::string& dir_name)
	{
		int result = _mkdir(dir_name.c_str());
		return result == 0 ? Tools::Result::Ok() : Tools::Result(Tools::ResultCode::CreateFolderError);
	}
}

#endif
#elif defined(_LINUX)




#include "sys/stat.h"

namespace Directory {
	Tools::Result CreateFolder(const std::string& dir_name)
	{
		int result = ::mkdir(dir_name.c_str(), 0777);
		return result == 0 ? Tools::Result::Ok() : Tools::Result(Tools::ResultCode::CreateFolderError);
	}
}
#else




namespace Directory {
	Tools::Result CreateFolder(const std::string& dir_name)
	{
		return Tools::Result(Tools::ResultCode::NotImplementedError);
	}
}

#endif





#ifdef _WIN32
#include <direct.h>
#define GETCWD _getcwd
#else
#include <unistd.h>
#define GETCWD getcwd
#endif

namespace CommandLine {

	string argv0;            
	string binaryDirectory;  
	string workingDirectory; 

	void init(int argc, char* argv[]) {
		(void)argc;
		string pathSeparator;

		
		argv0 = argv[0];

#ifdef _WIN32
		pathSeparator = "\\";
#ifdef _MSC_VER
		
		
		char* pgmptr = nullptr;
		if (!_get_pgmptr(&pgmptr) && pgmptr != nullptr && *pgmptr)
			argv0 = pgmptr;
#endif
#else
		pathSeparator = "/";
#endif

		
		workingDirectory = "";
		char buff[40000];
		char* cwd = GETCWD(buff, 40000);
		if (cwd)
			workingDirectory = cwd;

		
		binaryDirectory = argv0;
		size_t pos = binaryDirectory.find_last_of("\\/");
		if (pos == std::string::npos)
			binaryDirectory = "." + pathSeparator;
		else
			binaryDirectory.resize(pos + 1);

		
		if (binaryDirectory.find("." + pathSeparator) == 0)
			binaryDirectory.replace(0, 1, workingDirectory);
	}

}
