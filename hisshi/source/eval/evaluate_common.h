#ifndef _EVALUATE_COMMON_H_
#define _EVALUATE_COMMON_H_


#if defined (EVAL_KPPT) || defined(EVAL_KPP_KKPT) || defined(EVAL_NNUE)
#include <functional>

#define KK_BIN "KK_synthesized.bin"

#define KKP_BIN "KKP_synthesized.bin"

#define KPP_BIN "KPP_synthesized.bin"

namespace Eval
{

#if defined(USE_EVAL_HASH)
	void prefetch_evalhash(const Key key);
#endif

	void foreach_eval_param(std::function<void(s32, s32)>f, int type = -1);


#if defined(EVAL_LEARN)
	void init_grad(double eta1, u64 eta_epoch, double eta2, u64 eta2_epoch, double eta3);

	void add_grad(Position& pos, Color rootColor, double delt_grad, const std::array<bool, 4>& freeze);

	void update_weights(u64 epoch, const std::array<bool,4>& freeze);

	void save_eval(std::string suffix);

	double get_eta();


	void regularize_kk();

#endif


}


#endif

#endif 