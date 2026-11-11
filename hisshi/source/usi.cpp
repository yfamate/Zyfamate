#include "types.h"
#include "usi.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "tt.h"

#include <sstream>
#include <queue>

using namespace std;


#if defined(ENABLE_TEST_CMD)


namespace Test
{
	bool normal_test_cmd(Position& pos, std::istringstream& is, const std::string& token);

	bool mate_test_cmd(Position& pos, std::istringstream& is, const std::string& token);

	void test_cmd(Position& pos, std::istringstream& is)
	{
		is_ready();

		std::string token;
		is >> token;


		if (normal_test_cmd(pos, is, token))
			return;

		if (mate_test_cmd(pos,is,token))
			return;

		sync_cout << "Error! : unknown command = " << token << sync_endl;
	}

}

#endif 




void user_test(Position& pos, std::istringstream& is);

#if defined(ENABLE_TEST_CMD)
	void generate_moves_cmd(Position& pos);
#endif

#if defined(USE_MATE_DFPN)
void mate_cmd(Position& pos, istream& is);
#endif


#if defined (ENABLE_MAKEBOOK_CMD) && (defined(EVAL_LEARN) || defined(YANEURAOU_ENGINE_DEEP))
namespace Book { extern void makebook_cmd(Position& pos, istringstream& is); }
#endif


#if defined (EVAL_LEARN)
namespace Learner
{
  void gen_sfen(Position& pos, istringstream& is);

  void learn(Position& pos, istringstream& is);

#if defined(GENSFEN2019)
  void gen_sfen2019(Position& pos, istringstream& is);
#endif

  typedef std::pair<Value, std::vector<Move> > ValueAndPV;

  ValueAndPV qsearch(Position& pos);
  ValueAndPV search(Position& pos, int depth_, size_t multiPV = 1 , u64 nodesLimit = 0 );

}
#endif


extern void bench_cmd(Position& pos, istringstream& is);


#if defined(USE_GAMEOVER_HANDLER)
void gameover_handler(const string& cmd);
#endif


namespace USI
{

	std::string pv(const Position& pos, Depth depth, Value alpha, Value beta)
	{
		std::stringstream ss;
		TimePoint elapsed = Time.elapsed() + 1;

		const auto& rootMoves = pos.this_thread()->rootMoves;
		size_t pvIdx = pos.this_thread()->pvIdx;
		size_t multiPV = std::min((size_t)Options["MultiPV"], rootMoves.size());

		uint64_t nodes_searched = Threads.nodes_searched();

		for (size_t i = 0; i < multiPV; ++i)
		{
			bool updated = rootMoves[i].score != -VALUE_INFINITE;

			if (depth == 1 && !updated && i > 0)
				continue;

			Depth d = updated ? depth : std::max(1, depth - 1);
			Value v = updated ? rootMoves[i].score : rootMoves[i].previousScore;

			if (v == -VALUE_INFINITE)
				v = VALUE_ZERO; 


			if (ss.rdbuf()->in_avail()) 
				ss << endl;

			ss  << "info"
				<< " depth "    << d
				<< " seldepth " << rootMoves[i].selDepth
#if defined(USE_PIECE_VALUE)
				<< " score "    << USI::value(v)
#endif
				;

			if (i == pvIdx)
				ss << (v >= beta ? " lowerbound" : v <= alpha ? " upperbound" : "");

			if (multiPV > 1)
				ss << " multipv " << (i + 1);

			ss << " nodes " << nodes_searched
			   << " nps "   << nodes_searched * 1000 / elapsed;

			if (elapsed > 1000)
				ss << " hashfull " << TT.hashfull();

			ss << " time " << elapsed
			   << " pv";



			auto out_array_pv = [&]()
			{
				for (Move m : rootMoves[i].pv)
					ss << " " << m;
			};

			auto out_tt_pv = [&]()
			{
				auto pos_ = const_cast<Position*>(&pos);
				Move moves[MAX_PLY + 1];
				StateInfo si[MAX_PLY];
				int ply = 0;

				while ( ply < MAX_PLY )
				{
					auto rep = pos.is_repetition(ply);
					if (rep != REPETITION_NONE && ply >= 1)
					{
						ss << " " << rep;
						break;
					}

					Move m;

					if (ply == 0)
						m = rootMoves[i].pv[0];
					else
					{
						bool found;
						auto* tte = TT.read_probe(pos.state()->key(), found);

						if (!found)
							break;

						m = pos.to_move(tte->move());

						if (m != MOVE_WIN)
						{
							if (!(pos.pseudo_legal(m) && pos.legal(m)))
								break;
						}
					}

#if defined (USE_ENTERING_KING_WIN)
					if (m == MOVE_WIN)
					{
						if (pos.DeclarationWin() != MOVE_NONE)
							ss << " " << MOVE_WIN;

						break;
					}
#endif

					moves[ply] = m;
					ss << " " << m;

					pos_->do_move(m, si[ply]);
					++ply;
				}
				while (ply > 0)
					pos_->undo_move(moves[--ply]);
			};

			if (Search::Limits.consideration_mode)
				out_tt_pv();
			else
				out_array_pv();
		}

		return ss.str();
	}
}


u64 eval_sum;

void is_ready(bool skipCorruptCheck)
{




	auto thread_started = false;

	auto thread_end = false;

	auto th = std::thread([&] {
		thread_started = true;

		int count = 0;
		while (!thread_end)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			if (++count >= 50 )
			{
				count = 0;
				sync_cout << sync_endl; 

			}
		}
		});
	SCOPE_EXIT({ thread_end = true; th.join(); });

	while (!thread_started)
		Tools::sleep(100);



#if defined(YANEURAOU_ENGINE_DEEP)

	int threads_num =
		(int)Options["UCT_Threads1"] + (int)Options["UCT_Threads2"] + (int)Options["UCT_Threads3"] + (int)Options["UCT_Threads4"] +
		(int)Options["UCT_Threads5"] + (int)Options["UCT_Threads6"] + (int)Options["UCT_Threads7"] + (int)Options["UCT_Threads8"];

	Threads.set(std::max(threads_num,1));
#else
	Threads.set(size_t(Options["Threads"]));
#endif

#if defined (USE_EVAL_HASH)
	Eval::EvalHash_Resize(Options["EvalHash"]);
#endif


#if defined(YANEURAOU_ENGINE_DEEP)

	Eval::load_eval();
	USI::load_eval_finished = true;

#else

	if (!USI::load_eval_finished)
	{
		Eval::load_eval();

		eval_sum = Eval::calc_check_sum();

		Eval::print_softname(eval_sum);

		USI::load_eval_finished = true;
	}
	else
	{
		if (!skipCorruptCheck && eval_sum != Eval::calc_check_sum())
			sync_cout << "Error! : EVAL memory is corrupted" << sync_endl;
	}
#endif


#if !defined(USER_ENGINE)
	TT.resize(size_t(Options["USI_Hash"]));
#endif

	Search::clear();

#if defined (USE_EVAL_HASH)
	Eval::EvalHash_Clear();
#endif

	Threads.stop = false;
}

void is_ready_cmd(Position& pos, StateListPtr& states)
{


	is_ready();


	states = StateListPtr(new StateList(1));
	pos.set_hirate(&states->back(),Threads.main());

	sync_cout << "readyok" << sync_endl;
}

void position_cmd(Position& pos, istringstream& is , StateListPtr& states)
{
	Move m;
	string token, sfen;

	is >> token;

	if (token == "startpos")
	{
		sfen = SFEN_HIRATE;
		is >> token; 
	}
	else {
		if (token != "sfen")
			sfen += token + " ";
		while (is >> token && token != "moves")
			sfen += token + " ";
	}

	states = StateListPtr(new StateList(1));
	pos.set(sfen , &states->back() , Threads.main());

	std::vector<Move> moves_from_game_root;

	while (is >> token && (m = USI::to_move(pos, token)) != MOVE_NONE)
	{
		states->emplace_back();
		if (m == MOVE_NULL) 
			pos.do_null_move(states->back());
		else
			pos.do_move(m, states->back());

		moves_from_game_root.emplace_back(m);
	}

	Threads.main()->game_root_sfen = sfen;
	Threads.main()->moves_from_game_root = std::move(moves_from_game_root);

	Threads.main()->position_is_dirty = false;
}

void setoption_cmd(istringstream& is)
{
	string token, name, value;

	while (is >> token && token != "value")
		if (token != "name")
			name += (name.empty() ? "" : " ") + token;

	while (is >> token)
		value += (value.empty() ? "" : " ") + token;

	if (Options.count(name))
		Options[name] = value;
	else
		sync_cout << "Error! : No such option: " << name << sync_endl;

}

void getoption_cmd(istringstream& is)
{
	string name = "";
	is >> name;

	bool all = name == "";

	for (auto& o : Options)
	{
		if ((!StringExtension::stricmp(name, o.first)) || all)
		{
			sync_cout << "Options[" << o.first << "] == " << (string)Options[o.first] << sync_endl;
			if (!all)
				return;
		}
	}
	if (!all)
		sync_cout << "No such option: " << name << sync_endl;
}


void go_cmd(const Position& pos, istringstream& is , StateListPtr& states , bool ignore_ponder = false) {

	if (!USI::load_eval_finished)
	{
		sync_cout << "Error! go cmd before isready cmd." << sync_endl;
		return;
	}

	Search::LimitsType limits;
	string token;
	bool ponderMode = false;

	auto main_thread = Threads.main();
	if (!states)
	{
		istringstream iss(main_thread->last_position_cmd_string);
		iss >> token; 
		position_cmd(*const_cast<Position*>(&pos), iss, states);
	}

	Time.reset();


	int max_game_ply = 0;
	if (Options.count("MaxMovesToDraw"))
		max_game_ply = (int)Options["MaxMovesToDraw"];
	limits.max_game_ply = (max_game_ply == 0) ? 100000 : max_game_ply;

#if defined (USE_ENTERING_KING_WIN)
	limits.enteringKingRule = to_entering_king_rule(Options["EnteringKingRule"]);
#endif

#if defined (USE_GENERATE_ALL_LEGAL_MOVES)
	limits.generate_all_legal_moves = Options["GenerateAllLegalMoves"];
#endif


	limits.depth = Options.count("DepthLimit") ? (int)Options["DepthLimit"] : 0;
	limits.nodes = Options.count("NodesLimit") ? (u64)Options["NodesLimit"] : 0;

	while (is >> token)
	{
		if (token == "searchmoves")
			while (is >> token)
				limits.searchmoves.push_back(USI::to_move(pos, token));

		else if (token == "wtime")     is >> limits.time[WHITE];
		else if (token == "btime")     is >> limits.time[BLACK];

		else if (token == "winc")      is >> limits.inc[WHITE];
		else if (token == "binc")      is >> limits.inc[BLACK];

		else if (token == "rtime")     is >> limits.rtime;

		else if (token == "byoyomi") {
			TimePoint t = 0;
			is >> t;



			limits.byoyomi[BLACK] = limits.byoyomi[WHITE] = t;
		}
		else if (token == "depth")     is >> limits.depth;

		else if (token == "nodes")     is >> limits.nodes;

		else if (token == "movetime")  is >> limits.movetime;

		else if (token == "mate") {
			limits.mate = INT32_MAX;
			is >> std::ws;
			const auto argument_pos = is.tellg();
			string argument;
			if (is >> argument) {
				if (argument != "infinite") {
					if (!argument.empty() && argument.find_first_not_of("0123456789") == string::npos) {
						TimePoint duration = 0;
						for (const char digit : argument)
							duration = std::min<TimePoint>(INT32_MAX, duration * 10 + digit - '0');
						limits.mate = std::max<TimePoint>(1, duration);
					} else {
						is.seekg(argument_pos);
					}
				}
			}
		}

#if defined(TANUKI_MATE_ENGINE)

		else if (token == "matedebug") {
			string token="";
			Move16 m;
			limits.pv_check.clear();
			while (is >> token && (m = USI::to_move16(token)) != MOVE_NONE){
				limits.pv_check.push_back(m);
			}
		}
#endif

		else if (token == "perft")		is >> limits.perft;

		else if (token == "infinite")	limits.infinite = 1;

		else if (token == "ponder" && !ignore_ponder) {
			ponderMode = true;

			if (Options["Stochastic_Ponder"] && main_thread->moves_from_game_root.size() >= 1)
			{

				auto m = main_thread->moves_from_game_root.back();
				main_thread->moves_from_game_root.pop_back();
				const_cast<Position*>(&pos)->undo_move(m);
				states->pop_back();
				main_thread->position_is_dirty = true;
			}
		}
	}

	if (limits.nodes == 0 && limits.byoyomi[BLACK] == 0 && limits.inc[BLACK] == 0 && limits.time[BLACK] == 0 && limits.rtime == 0)
		limits.byoyomi[BLACK] = limits.byoyomi[WHITE] = 1000;

	Threads.start_thinking(pos, states , limits , ponderMode);
}


#if defined(EVAL_LEARN)
void qsearch_cmd(Position& pos)
{
	cout << "qsearch : ";
	auto pv = Learner::qsearch(pos);
	cout << "Value = " << pv.first << " , PV = ";
	for (auto m : pv.second)
		cout << m << " ";
	cout << endl;
}

void search_cmd(Position& pos, istringstream& is)
{
	string token;
	int depth = 1;
	int multi_pv = (int)Options["MultiPV"];
	while (is >> token)
	{
		if (token == "depth")
			is >> depth;
		if (token == "multipv")
			is >> multi_pv;
	}

	cout << "search depth = " << depth << " , multi_pv = " << multi_pv << " : ";
	auto pv = Learner::search(pos , depth , multi_pv);
	cout << "Value = " << pv.first << " , PV = ";
	for (auto m : pv.second)
		cout << m << " ";
	cout << endl;
}

#endif


void USI::loop(int argc, char* argv[])
{
	Position pos;

	string cmd, token;

	StateListPtr states(new StateList(1));

	queue<string> cmds;

	if (argc >= 3 && string(argv[1]) == "file")
	{
		vector<string> cmds0;
		SystemIO::ReadAllLines(argv[2], cmds0);

		for (auto c : cmds0)
			cmds.push(c);

	} else {


		for (int i = 1; i < argc; ++i)
		{
			string s = argv[i];

			while (*s.rbegin() == ' ') s.pop_back();
			while (*s.begin() == ' ') s = s.substr(1, s.size() - 1);

			if (s != ",")
				cmd += s + " ";
			else
			{
				cmds.push(cmd);
				cmd = "";
			}
		}
		if (cmd.size() != 0)
			cmds.push(cmd);
	}

	do
	{
		if (cmds.size() == 0)
		{
			if (!std::getline(cin, cmd)) 
				cmd = "quit";
		} else {
			cmd = cmds.front();
			cmds.pop();
		}

		istringstream is(cmd);

		token.clear(); 
		is >> skipws >> token;

		if (token == "quit" || token == "stop" || token == "gameover")
		{

#if defined(USE_GAMEOVER_HANDLER)
			if (token == "gameover")
				gameover_handler(cmd);
#endif

			Threads.stop = true;

		} else if (token == "ponderhit")
		{
			if (Options["Stochastic_Ponder"])
			{

				auto org = Search::Limits.silent;
				Search::Limits.silent = true;
				Threads.stop = true;
				Threads.main()->wait_for_search_finished();
				Search::Limits.silent = org;

				istringstream iss(Threads.main()->last_go_cmd_string);
				go_cmd(pos, iss, states, true);
			}
			else {
				Time.reset_for_ponderhit(); 
				Threads.main()->ponder = false; 
			}
		}

		else if (token == "usi")
			sync_cout << engine_info() << Options << "usiok" << sync_endl;

		else if (token == "setoption") setoption_cmd(is);

		else if (token == "go") {
			Threads.main()->last_go_cmd_string = cmd;       
			go_cmd(pos, is, states);
		}

		else if (token == "position") {
			Threads.main()->last_position_cmd_string = cmd; 
			position_cmd(pos, is, states);
		}

		else if (token == "usinewgame") continue;

		else if (token == "isready") is_ready_cmd(pos,states);


#if defined(USER_ENGINE)
		else if (token == "user") user_test(pos, is);
#endif

		else if (token == "bench") bench_cmd(pos, is);

		else if (token == "d") cout << pos << endl;

		else if (token == "eval") cout << "eval = " << Eval::compute_eval(pos) << endl;
		else if (token == "evalstat") Eval::print_eval_stat(pos);

		else if (token == "compiler") sync_cout << compiler_info() << sync_endl;


		else if (token == "getoption") getoption_cmd(is);

		else if (token == "matsuri") pos.set("l6nl/5+P1gk/2np1S3/p1p4Pp/3P2Sp1/1PPb2P1P/P5GS1/R8/LN4bKL w GR5pnsg 1", &states->back(), Threads.main());

		else if (token == "sfen") position_cmd(pos, is, states);

		else if (token == "log") start_logger(true);

#if defined(EVAL_LEARN)
		else if (token == "qsearch") qsearch_cmd(pos);
		else if (token == "search") search_cmd(pos,is);
#endif

		else if (token == "moves") {
			for (auto m : MoveList<LEGAL_ALL>(pos))
				cout << m.move << ' ';
			cout << endl;
		}

		else if (token == "side") cout << (pos.side_to_move() == BLACK ? "black":"white") << endl;

		else if (token == "mated") cout << pos.is_mated() << endl;

		else if (token == "key") cout << hex << pos.state()->key() << dec << endl;

		else if (token == "wait") Threads.main()->wait_for_search_finished();

		else if (token == "sleep") { u64 ms; is >> ms; Tools::sleep(ms); }

#if defined(MATE_1PLY) && defined(LONG_EFFECT_LIBRARY)
		else if (token == "mate1") cout << pos.mate1ply() << endl;
#endif

#if defined (ENABLE_TEST_CMD)
		else if (token == "test") Test::test_cmd(pos, is);
#endif

#if defined (ENABLE_MAKEBOOK_CMD) && (defined(EVAL_LEARN) || defined(YANEURAOU_ENGINE_DEEP))
		else if (token == "makebook") Book::makebook_cmd(pos, is);
#endif

#if defined (EVAL_LEARN)
		else if (token == "gensfen") Learner::gen_sfen(pos, is);
		else if (token == "learn") Learner::learn(pos, is);

#if defined (GENSFEN2019)
		else if (token == "gensfen2019") Learner::gen_sfen2019(pos, is);
#endif

#endif

		else
		{

			if (!token.empty())
			{
				string value;
				is >> value;

				for (auto& o : Options)
				{
					if (!StringExtension::stricmp(token, o.first))
					{
						Options[o.first] = value;
						sync_cout << "Options[" << o.first << "] = " << value << sync_endl;

						goto OPTION_FOUND;
					}
				}
				sync_cout << "No such option: " << token << sync_endl;
			OPTION_FOUND:;
			}
		}

	} while (token != "quit");

	Threads.main()->wait_for_search_finished();
}


namespace {
	Square usi_to_sq(char f, char r)
	{
		File file = toFile(f);
		Rank rank = toRank(r);

		if (is_ok(file) && is_ok(rank))
			return file | rank;

		return SQ_NB;
	}
}

#if defined(USE_PIECE_VALUE)
std::string USI::value(Value v)
{
	ASSERT_LV3(-VALUE_INFINITE < v && v < VALUE_INFINITE);

	std::stringstream s;

	if (v == VALUE_NONE)
		s << "none";
	else if (abs(v) < VALUE_MATE_IN_MAX_PLY)
		s << "cp " << v * 100 / int(Eval::PawnValue);
	else if (v == -VALUE_MATE)
		s << "mate -0";
	else
		s << "mate " << (v > 0 ? VALUE_MATE - v : -VALUE_MATE - v);

	return s.str();
}
#endif

std::string USI::square(Square s) {
	return std::string{ char('a' + file_of(s)), char('1' + rank_of(s)) };
}

std::string USI::move(Move   m) { return move(Move16(m)); }
std::string USI::move(Move16 m)
{
	std::stringstream ss;
	if (!is_ok(m))
	{
		ss << ((m == MOVE_RESIGN) ? "resign" :
			   (m == MOVE_WIN)    ? "win" :
			   (m == MOVE_NULL)   ? "null" :
			   (m == MOVE_NONE)   ? "none" :
			    "");
	}
	else if (is_drop(m))
	{
		ss << move_dropped_piece(m);
		ss << '*';
		ss << to_sq(m);
	}
	else {
		ss << from_sq(m);
		ss << to_sq(m);
		if (is_promote(m))
			ss << '+';
	}
	return ss.str();
}

std::string USI::move(const std::vector<Move>& moves)
{
	std::ostringstream oss;
	for (const auto& move : moves) {
		oss << " " << move;
	}
	return oss.str();
}


Move USI::to_move(const Position& pos, const std::string& str)
{


	if (str == "resign")
		return MOVE_RESIGN;

	if (str == "win")
		return MOVE_WIN;

	if (str == "0000" || str == "null" || str == "pass")
		return MOVE_NULL;

	Move move = pos.to_move(USI::to_move16(str));

#if defined(MUST_CAPTURE_SHOGI_ENGINE)
	if (!MoveList<LEGAL>(pos).contains(move))
		sync_cout << "info string Error!! Illegal Move = " << move << sync_endl;
#endif

	if (pos.pseudo_legal(move) && pos.legal(move))
		return move;


	return MOVE_NONE;
}


Move16 USI::to_move16(const string& str)
{
	Move16 move = MOVE_NONE;

	{
		if (str.length() <= 3)
			goto END;

		Square to = usi_to_sq(str[2], str[3]);
		if (!is_ok(to))
			goto END;

		bool promote = str.length() == 5 && str[4] == '+';
		bool drop = str[1] == '*';

		if (!drop)
		{
			Square from = usi_to_sq(str[0], str[1]);
			if (is_ok(from))
				move = promote ? make_move_promote16(from, to) : make_move16(from, to);
		}
		else
		{
			for (int i = 1; i <= 7; ++i)
				if (PieceToCharBW[i] == str[0])
				{
					move = make_move_drop16((PieceType)i, to);
					break;
				}
		}
	}

END:
	return move;
}
