#include "thread.h"
#include "tt.h"
#include "usi.h"
#include "misc.h"

using std::string;

USI::OptionsMap Options;

namespace USI {


	std::vector<std::string> ekr_rules = { "NoEnteringKing", "CSARule24" , "CSARule27" , "TryRule" };

	void read_engine_options();

	bool CaseInsensitiveLess::operator() (const string& s1, const string& s2) const {

		return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(),
			[](char c1, char c2) { return tolower(c1) < tolower(c2); });
	}

	std::string last_eval_dir;

	void init(OptionsMap& o)
	{
		constexpr int MaxHashMB = Is64Bit ? 33554432 : 2048;


#if !defined(YANEURAOU_ENGINE_DEEP)


#if defined(USER_ENGINE)
		o["Threads"] << Option(1, 1, 512, [](const Option& o) {  });
#else
		o["Threads"] << Option(4, 1, 512, [](const Option& o) {  });
#endif
#endif

#if !defined(TANUKI_MATE_ENGINE) && !defined(YANEURAOU_MATE_ENGINE) && !defined(USER_ENGINE)
		o["USI_Hash"] << Option(16, 1, MaxHashMB, [](const Option&o) {  });

	#if defined(USE_EVAL_HASH)

		#if defined(FOR_TOURNAMENT)
				o["EvalHash"] << Option(1024, 1, MaxHashMB, [](const Option& o) { Eval::EvalHash_Resize(o); });
		#else
				o["EvalHash"] << Option(128, 1, MaxHashMB, [](const Option& o) { Eval::EvalHash_Resize(o); });
		#endif 
	#endif 

		o["USI_Ponder"] << Option(false);

		o["Stochastic_Ponder"] << USI::Option(false);

		o["MultiPV"] << Option(1, 1, 800);

	#if defined(YANEURAOU_ENGINE_DEEP)
			int time_margin = 400;
	#else
			int time_margin = 120;
	#endif

		o["NetworkDelay"] << Option(time_margin, 0, 10000);

		o["NetworkDelay2"] << Option(time_margin + 1000, 0, 10000);

		o["MinimumThinkingTime"] << Option(2000, 1000, 100000);

		o["SlowMover"] << Option(100, 1, 1000);

		o["MaxMovesToDraw"] << Option(0, 0, 100000);

		o["DepthLimit"] << Option(0, 0, INT_MAX);

		o["NodesLimit"] << Option(0, 0, INT64_MAX);

		last_eval_dir = "eval";
		o["EvalDir"] << Option("eval", [](const USI::Option&o) {
			if (last_eval_dir != string(o))
			{
				last_eval_dir = string(o);
				load_eval_finished = false;
			}
		});

#else

#if defined(USER_ENGINE)
		o["USI_Hash"] << Option(1024, 1, MaxHashMB);
#else
		o["USI_Hash"] << Option(4096, 1, MaxHashMB);
#endif

#endif 

#if !defined(USER_ENGINE)
		o["WriteDebugLog"] << Option(false, [](const Option& o) { start_logger(o); });
#endif


#if defined (USE_ENTERING_KING_WIN)
		o["EnteringKingRule"] << Option(USI::ekr_rules, USI::ekr_rules[EKR_27_POINT]);
#endif

#if defined(USE_GENERATE_ALL_LEGAL_MOVES)
		o["GenerateAllLegalMoves"] << Option(false);
#endif


#if defined (USE_SHARED_MEMORY_IN_EVAL) && defined(_WIN32) && \
	 (defined(EVAL_KPPT) || defined(EVAL_KPP_KKPT) )
		o["EvalShare"] << Option(true);
#endif

#if defined(EVAL_LEARN)
		o["SkipLoadingEval"] << Option(false);
#endif

#if defined(_WIN32) && !defined(USER_ENGINE)

		o["ThreadIdOffset"] << Option(0, 0, std::thread::hardware_concurrency() - 1);
#endif

#if defined(_WIN64) && !defined(USER_ENGINE)
		o["LargePageEnable"] << Option(true);
#endif

		USI::extra_option(o);

		read_engine_options();
	}

	std::ostream& operator<<(std::ostream& os, const OptionsMap& om)
	{
		for (size_t idx = 0; idx < om.size(); ++idx)
			for (const auto& it : om)
				if (it.second.idx == idx)
				{
					const Option& o = it.second;
					os << "option name " << it.first << " type " << o.type;

					if (o.type == "string" || o.type == "check" || o.type == "combo")
						os << " default " << o.defaultValue;

					if (o.type == "spin")
						os << " default " << s64(stoll(o.defaultValue))
						<< " min " << o.min
						<< " max " << o.max;

					if (o.list.size())
						for (auto v : o.list)
							os << " var " << v;

					os << std::endl;

					break;
				}

		return os;
	}


	Option::Option(const char* v, OnChange f) : type("string"), min(0), max(0), on_change(f)
	{
		defaultValue = currentValue = v;
	}

	Option::Option(bool v, OnChange f) : type("check"), min(0), max(0), on_change(f)
	{
		defaultValue = currentValue = (v ? "true" : "false");
	}

	Option::Option(OnChange f) : type("button"), min(0), max(0), on_change(f)
	{}

	Option::Option(s64 v, s64 minv, s64 maxv, OnChange f) : type("spin"), min(minv), max(maxv), on_change(f)
	{
		defaultValue = currentValue = std::to_string(v);
	}

	Option::Option(const std::vector<std::string>&list, const std::string& v, OnChange f)
		: type("combo"), on_change(f), list(list)
	{
		defaultValue = currentValue = v;
	}

	Option::operator s64() const {
		ASSERT_LV1(type == "check" || type == "spin");
		return (type == "spin" ? stoll(currentValue) : currentValue == "true");
	}

	Option::operator std::string() const {
		return currentValue;
	}

	bool Option::operator==(const char* s) const {
		ASSERT_LV1(type == "combo");
		return    !CaseInsensitiveLess()(currentValue, s)
			   && !CaseInsensitiveLess()(s, currentValue);
	}

	void Option::operator<<(const Option& o)
	{
		static size_t insert_order = 0;
		*this = o;
		idx = insert_order++; 
	}

	Option& Option::operator=(const string& v) {

		ASSERT_LV1(!type.empty());

		if (  ((type != "button" && type != "string") && v.empty())
			|| (type == "check" && v != "true" && v != "false")
			|| (type == "spin" && (stoll(v) < min || stoll(v) > max)))
			return *this;

		if (type != "button")
			currentValue = v;

		if (on_change)
			on_change(*this);

		return *this;
	}


	bool load_eval_finished = false;

#if defined(USE_ENTERING_KING_WIN)

	EnteringKingRule to_entering_king_rule(const std::string& rule)
	{
		for (size_t i = 0; i < ekr_rules.size(); ++i)
			if (ekr_rules[i] == rule)
				return (EnteringKingRule)i;

		ASSERT(false);
		return EnteringKingRule::EKR_NONE;
	}
#endif

	void build_option(string line)
	{
		Parser::LineScanner scanner(line);
		if (scanner.get_text() != "option") return;

		string name, value, option_type;
		int64_t min_value = 0, max_value = 1;
		std::vector<string> combo_list;
		while (!scanner.eol())
		{
			auto token = scanner.get_text();
			if (token == "name") name = scanner.get_text();
			else if (token == "type") option_type = scanner.get_text();
			else if (token == "default") value = scanner.get_text();
			else if (token == "min") min_value = stoll(scanner.get_text());
			else if (token == "max") max_value = stoll(scanner.get_text());
			else if (token == "var") {
				auto varText = scanner.get_text();
				combo_list.push_back(varText);
			}
			else {
				std::cout << "Error : invalid command: " << token << std::endl;
			}
		}

		if (Options.count(name) != 0)
		{
			if (option_type == "check") Options[name].overwrite(Option(value == "true"));
			else if (option_type == "spin") Options[name].overwrite(Option(stoll(value), min_value, max_value));
			else if (option_type == "string") Options[name].overwrite(Option(value.c_str()));
			else if (option_type == "combo") Options[name].overwrite(Option(combo_list, value));
		}
		else
			std::cout << "Error : option name not found : " << name << std::endl;

	}

	void read_engine_options()
	{
		std::ifstream ifs("engine_options.txt");
		if (!ifs.fail())
		{
			std::string str;
			while (Tools::getline(ifs, str))
				build_option(str);
		}
	}

	void Option::overwrite(const Option& o)
	{
		bool modified = this->currentValue != o.currentValue;

		auto fn = this->on_change;
		auto idx_ = idx;

		*this = o;

		idx = idx_;
		this->on_change = fn;

		if (modified && fn)
			fn(*this);
	}

} 

