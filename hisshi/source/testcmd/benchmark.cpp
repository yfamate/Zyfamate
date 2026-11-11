#include "../types.h"

#include <sstream>
#include "../tt.h"
#include "../search.h"
#include "../thread.h"
#include "../usi.h"

using namespace std;


extern void position_cmd(Position & pos, istringstream & is, StateListPtr & states);

const vector<string> BenchSfen =
{

	"sfen l4S2l/4g1gs1/5p1p1/pr2N1pkp/4Gn3/PP3PPPP/2GPP4/1K7/L3r+s2L w BS2N5Pb 1",

	"sfen 6n1l/2+S1k4/2lp4p/1np1B2b1/3PP4/1N1S3rP/1P2+pPP+p1/1p1G5/3KG2r1 b GSN2L4Pgs2p 1",

	"sfen l6nl/5+P1gk/2np1S3/p1p4Pp/3P2Sp1/1PPb2P1P/P5GS1/R8/LN4bKL w RGgsn5p 1",
};

void bench_cmd(Position& current, istringstream& is)
{
	auto oldOptions = Options;

	std::string token;
	Search::LimitsType limits;
	vector<std::string> fens;

	std::string ttSize = "1024", threads  ="1", limit ="17" , fenFile ="default", limitType = "depth";
	string* positional_args[] = { &ttSize, &threads, &limit, &fenFile, &limitType };

#if defined(YANEURAOU_ENGINE_DEEP)
	limitType = "nodes";
	limit = "200000";
#endif


	int p = 0;
	while (is >> token)
	{
		if (token == "hash")
			is >> ttSize;
		else if (token == "threads")
			is >> threads;
		else if (token == "limit")
			is >> limit;
		else if (token == "file")
			is >> fenFile;
		else if (token == "type")
			is >> limitType;
		else
		{
			if (p <  5)
				*positional_args[p++] = token;
		}
	}

	if (ttSize == "d")
	{
		ttSize = "1024";
		threads = "1";
		fenFile = "default";
		limitType = "depth";
		limit = "6";
	}

	if (Options.count("Threads"))
		Options["Threads"] = threads;

	if (Options.count("UCT_Threads1"))
		Options["UCT_Threads1"] = threads;

	if (limitType == "time")
		limits.movetime = (TimePoint)1000 * stoi(limit); 

	else if (limitType == "nodes")
		limits.nodes = stoll(limit);

	else if (limitType == "mate")
		limits.mate = stoi(limit);

	else
		limits.depth = stoi(limit);

	if (Options.count("USI_Hash"))
		Options["USI_Hash"] = ttSize;

	if (Options.count("BookFile"))
		Options["BookFile"] = string("no_book");

	limits.bench = true;

	limits.enteringKingRule = EKR_NONE;

	if (fenFile == "default")
		fens = BenchSfen;
	else if (fenFile == "current")
		fens.push_back(current.sfen());
	else
		SystemIO::ReadAllLines(fenFile, fens);

	is_ready();


	int64_t nodes = 0;

	int64_t nodes_main = 0;

	Timer time;
	time.reset();

	sync_cout << "Benchmark" << endl
			  << "    hash    : " << ttSize << endl
			  << "    threads : " << threads << endl
			  << "    limit   : " << limitType << " " << limit << endl
			  << "    sfen    : " << fenFile << sync_endl;

	Position pos;
	for (size_t i = 0; i < fens.size(); ++i)
	{
		StateListPtr states(new StateList(1));

		istringstream is(fens[i]);
		position_cmd(pos, is, states);

		sync_cout << "\nPosition: " << (i + 1) << '/' << fens.size() << sync_endl;

		Time.reset();

		Threads.start_thinking(pos, states , limits);
		Threads.main()->wait_for_search_finished(); 

		nodes += Threads.nodes_searched();
		nodes_main += Threads.main()->nodes.load(std::memory_order_relaxed);
	}

	auto elapsed = time.elapsed() + 1; 

	sync_cout << "\n==========================="
		<< "\nTotal time (ms) : " << elapsed
		<< "\nNodes searched  : " << nodes
		<< "\nNodes/second    : " << 1000 * nodes / elapsed;

	if (stoi(threads) > 1)
		cout
		<< "\nNodes searched(main thread) : " << nodes_main
		<< "\nNodes/second  (main thread) : " << 1000 * nodes_main / elapsed;

	cout << sync_endl;

	for (auto& s : oldOptions)
		Options[s.first] = std::string(s.second);
}


