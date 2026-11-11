#ifndef USI_H_INCLUDED
#define USI_H_INCLUDED

#include <map>
#include <vector>
#include <functional>	

#include "types.h"


class Position;

namespace USI
{
	class Option;

	struct CaseInsensitiveLess {
		bool operator() (const std::string&, const std::string&) const;
	};

	typedef std::map<std::string, Option , CaseInsensitiveLess> OptionsMap;

	class Option {

		typedef std::function<void(const Option&)> OnChange;

	public:
		Option(OnChange f = nullptr);
		
		Option(const char* v, OnChange f = nullptr);
		
		Option(bool v, OnChange f = nullptr);
		
		Option(s64 v, s64 minv, s64 maxv, OnChange = nullptr);
		
		Option(const std::vector<std::string>&list, const std::string& v, OnChange f = nullptr);

		Option& operator=(const std::string&);

		void operator<<(const Option&);

		operator s64() const;

		operator std::string() const;

		bool operator==(const char*) const;

		void overwrite(const Option&);

	private:
		friend std::ostream& operator<<(std::ostream& os, const OptionsMap& om);

		std::string defaultValue, currentValue, type;

		s64 min, max;

		size_t idx;

		std::vector<std::string> list;

		OnChange on_change;
	};

	void init(OptionsMap&);

	void loop(int argc, char* argv[]);

#if defined(USE_PIECE_VALUE)
	std::string value(Value v);
#endif

	std::string square(Square s);

	std::string move(Move   m );
	std::string move(Move16 m );

	std::string move(const std::vector<Move>& moves);

	std::string pv(const Position& pos, Depth depth, Value alpha, Value beta);

	Move to_move(const Position& pos, const std::string& str);


	Move16 to_move16(const std::string& str);

	std::ostream& operator<<(std::ostream& os, const OptionsMap& om);

	void extra_option(USI::OptionsMap& o);

	extern bool load_eval_finished; 

#if defined (USE_ENTERING_KING_WIN)
	extern EnteringKingRule to_entering_king_rule(const std::string& rule);
#endif

}

extern USI::OptionsMap Options;

void is_ready(bool skipCorruptCheck = false);

#endif 
