#ifndef MISC_H_INCLUDED
#define MISC_H_INCLUDED

#include <chrono>
#include <vector>
#include <functional>
#include <fstream>
#include <mutex>
#include <atomic>
#include <sstream>

#include "types.h"


const std::string engine_info();

const std::string compiler_info();



void prefetch(void* addr);


void start_logger(bool b);


struct LargeMemory
{
	void* alloc(size_t size, size_t align = 256 , bool zero_clear = false);

	void free();

	bool alloced() const { return mem != nullptr; }

	static void* static_alloc(size_t size, void*& mem, size_t align = 256, bool zero_clear = false);

	static void static_free(void* mem);

	~LargeMemory() { free(); }

private:
	void* mem = nullptr;
};


void dbg_hit_on(bool b);

void dbg_hit_on(bool c , bool b);

void dbg_mean_of(int v);

void dbg_print();



typedef std::chrono::milliseconds::rep TimePoint;
static_assert(sizeof(TimePoint) == sizeof(int64_t), "TimePoint should be 64 bits");

static TimePoint now() {
	return std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::steady_clock::now().time_since_epoch()).count();
}






enum SyncCout { IO_LOCK, IO_UNLOCK };
std::ostream& operator<<(std::ostream&, SyncCout);

#define sync_cout std::cout << IO_LOCK
#define sync_endl std::endl << IO_UNLOCK


struct PRNG
{
	PRNG(u64 seed) : s(seed) { ASSERT_LV1(seed); }

	PRNG() {

		s = (u64)(time(NULL)) + ((u64)(this) << 32)
			+ (u64)(std::chrono::high_resolution_clock::now().time_since_epoch().count());
	}

	template<typename T> T rand() { return T(rand64()); }

	u64 rand(u64 n) { return rand<u64>() % n; }

	u64 get_seed() const { return s;  }

private:
	u64 s;
	u64 rand64() {
		s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
		return s * 2685821657736338717LL;
	}
};

static std::ostream& operator<<(std::ostream& os, PRNG& prng)
{
	os << "PRNG::seed = " << std::hex << prng.get_seed() << std::dec;
	return os;
}


inline uint64_t mul_hi64(uint64_t a, uint64_t b) {
#if defined(__GNUC__) && defined(IS_64BIT)
	__extension__ typedef unsigned __int128 uint128;
	return ((uint128)a * (uint128)b) >> 64;
#else
	uint64_t aL = (uint32_t)a, aH = a >> 32;
	uint64_t bL = (uint32_t)b, bH = b >> 32;
	uint64_t c1 = (aL * bL) >> 32;
	uint64_t c2 = aH * bL + c1;
	uint64_t c3 = aL * bH + (uint32_t)c2;
	return aH * bH + (c2 >> 32) + (c3 >> 32);
#endif
}



namespace WinProcGroup {
	void bindThisThread(size_t idx);
}


namespace Search { struct LimitsType; }

struct Timer
{
	void reset() { startTime = startTimeFromPonderhit = now(); }

	void reset_for_ponderhit() { startTimeFromPonderhit = now(); }

	TimePoint elapsed() const;

	TimePoint elapsed_from_ponderhit() const;

	TimePoint elapsed_from_start_to_ponderhit() const { return (TimePoint)(startTimeFromPonderhit - startTime); }

#if 0
	TimePoint availableNodes;
#endif

#if defined(USE_TIME_MANAGEMENT)

	void init(const Search::LimitsType& limits, Color us, int ply);

	TimePoint minimum() const { return minimumTime; }
	TimePoint optimum() const { return optimumTime; }
	TimePoint maximum() const { return maximumTime; }

	TimePoint round_up(TimePoint t) const;

	std::atomic<TimePoint> search_end;

private:
	TimePoint minimumTime;
	TimePoint optimumTime;
	TimePoint maximumTime;

	TimePoint network_delay;
	TimePoint minimum_thinking_time;

	TimePoint remain_time;

#endif

private:
	TimePoint startTime;

	TimePoint startTimeFromPonderhit;
};

extern Timer Time;




namespace Tools
{
	extern void memclear(const char* name, void* table, size_t size);

	template <typename T >
	void insertion_sort(T* arr, int left, int right)
	{
		for (int i = left + 1; i < right; i++)
		{
			auto key = arr[i];
			int j = i - 1;

			while (j >= left && (arr[j] > key))
			{
				arr[j + 1] = arr[j];
				j = j - 1;
			}
			arr[j + 1] = key;
		}
	}

	extern void exit();

	extern void sleep(u64 ms);

	extern std::string now_string();

	extern bool getline(std::ifstream& fs, std::string& s);

	extern std::wstring MultiByteToWideChar(const std::string& s);

	struct __FINALLY__ {
		__FINALLY__(std::function<void()> fn_) : fn(fn_) {}
		~__FINALLY__() { fn(); }
	private:
		std::function<void()> fn;
	};


	enum struct ResultCode
	{
		Ok,

		Eof,

		SomeError,

		MemoryAllocationError,

		FileOpenError,

		FileReadError,

		FileWriteError,

		FileCloseError,

		CreateFolderError,

		NotImplementedError,
	};

	extern std::string to_string(ResultCode);

	struct Result
	{
		Result(ResultCode code_) : code(code_) {}

		ResultCode code;

		bool is_ok() const { return code == ResultCode::Ok; }

		bool is_not_ok() const { return code != ResultCode::Ok; }

		bool is_eof() const { return code == ResultCode::Eof; }

		std::string to_string() const { return Tools::to_string(code); }

		static Result Ok() { return Result(ResultCode::Ok); }
	};
}

#define SCOPE_EXIT(STATEMENT) Tools::__FINALLY__ __clean_up_object__([&]{ STATEMENT });



namespace SystemIO
{
	extern Tools::Result ReadAllLines(const std::string& filename, std::vector<std::string>& lines, bool trim = false);



	extern Tools::Result ReadFileToMemory(const std::string& filename, std::function<void* (size_t)> callback_func);
	extern Tools::Result WriteMemoryToFile(const std::string& filename, void* ptr, size_t size);

	struct TextReader
	{
		TextReader();
		~TextReader();

		Tools::Result Open(const std::string& filename);

		void Close();


		Tools::Result ReadLine(std::string& line);

		void SkipEmptyLine(bool skip = true) { skipEmptyLine = skip; }

		void SetTrim(bool trim = true) { this->trim = trim; }

	private:
		void clear();

		void read_next_block();

		FILE* fp;

		int read_char();

		Tools::Result read_line_simple();

		std::vector<u8> buffer;

		std::vector<u8> line_buffer;

		size_t read_size;

		size_t cursor;

		bool is_eof;

		bool is_prev_cr;

		bool trim;

		bool skipEmptyLine;
	};

	class BinaryBase
	{
	public:
		Tools::Result close();

		virtual ~BinaryBase() { close(); }

	protected:
		FILE* fp = nullptr;
	};

	class BinaryReader : public BinaryBase
	{
	public:
		Tools::Result open(const std::string& filename);

		size_t get_size();

		Tools::Result read(void* ptr , size_t size);
	};

	class BinaryWriter : public BinaryBase
	{
	public:
		Tools::Result open(const std::string& filename);

		Tools::Result write(void* ptr, size_t size);
	};
};


struct AsyncPRNG
{
	template<typename T> T rand() {
		std::unique_lock<std::mutex> lk(mutex);
		return prng.rand<T>();
	}

	u64 rand(u64 n) {
		std::unique_lock<std::mutex> lk(mutex);
		return prng.rand(n);
	}

	u64 get_seed() const { return prng.get_seed(); }

protected:
	std::mutex mutex;
	PRNG prng;
};

static std::ostream& operator<<(std::ostream& os, AsyncPRNG& prng)
{
	os << "AsyncPRNG::seed = " << std::hex << prng.get_seed() << std::dec;
	return os;
}


namespace Parser
{
	struct LineScanner
	{
		LineScanner(std::string line_) : line(line_), pos(0) {}

		std::string peek_text();

		std::string get_text();

		s64 get_number(s64 defaultValue);
		double get_double(double defaultValue);

		bool eol() const { return token.empty() && raw_eol(); }

	private:
		bool raw_eol() const { return !(pos < line.length()); }

		std::string line;

		unsigned int pos;

		std::string token;
	};

	class ArgumentParser
	{
	public:
		typedef std::pair<std::string , std::function<void(std::istringstream&)>> ArgPair;

		template<typename T>
		void add_argument(const std::string& arg_name, T& v)
		{
			auto f = [&](std::istringstream& is) { is >> v; };
			a.emplace_back(ArgPair(arg_name,f));
		}

		void parse_args(std::istringstream& is)
		{
			std::string token;
			while (is >> token)
				for (auto p : a)
					if (p.first == token)
					{
						p.second(is);
						break;
					}
		}

		std::vector <ArgPair> a;
	};
}


namespace Math {
	double sigmoid(double x);

	double dsigmoid(double x);

	template<class T> constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
		return v < lo ? lo : v > hi ? hi : v;
	}

	template<class T> constexpr const T align(const T v, const int c)
	{
		ASSERT_LV3((c & (c - 1)) == 0);
		return (v + (T)(c - 1)) & ~(T)(c - 1);
	}

}


namespace Path
{
	extern std::string Combine(const std::string& folder, const std::string& filename);

	extern std::string GetFileName(const std::string& path);

	extern std::string GetDirectoryName(const std::string& path);
};


namespace StringExtension
{
	extern bool stricmp(const std::string& s1, const std::string& s2);

	extern std::string trim(const std::string& input);

	extern void trim_inplace(std::string& input);

	extern std::string trim_number(const std::string& input);

	extern void trim_number_inplace(std::string& s);

	extern int to_int(const std::string input, int default_);

	extern std::vector<std::string> split(const std::string& input);


	extern bool StartsWith(std::string const& value, std::string const& starting);

	extern bool EndsWith(std::string const& value, std::string const& ending);

};


namespace Directory
{
	extern std::vector<std::string> EnumerateFiles(const std::string& sourceDirectory, const std::string& extension);

	extern Tools::Result CreateFolder(const std::string& dir_name);

	extern std::string GetCurrentFolder();
}

namespace CommandLine {
	extern void init(int argc, char* argv[]);

	extern std::string binaryDirectory;  
	extern std::string workingDirectory; 
}


#endif 
