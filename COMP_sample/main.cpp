//=====================================================================//
/*!	@file
	@brief	R8C メイン
	@author	平松邦仁 (hira@rvf-rc45.net)
*/
//=====================================================================//
#include "system.hpp"
#include "clock.hpp"
#include "port.hpp"
#include "intr.hpp"
#include "vdetect.hpp"
#include "common/vect.h"
#include "common/delay.hpp"
#include "common/port_map.hpp"
#include "common/intr_utils.hpp"
#include "common/trb_io.hpp"
#include "common/comp_io.hpp"

namespace {

	typedef device::trb_io<utils::null_task> timer_b;
	timer_b timer_b_;

	typedef device::comp_io<utils::null_task, utils::null_task> comp;
	comp comp_;

}

extern "C" {
	const void* variable_vectors_[] __attribute__ ((section (".vvec"))) = {
		reinterpret_cast<void*>(brk_inst_),		nullptr,	// (0)
		reinterpret_cast<void*>(null_task_),	nullptr,	// (1) flash_ready
		reinterpret_cast<void*>(null_task_),	nullptr,	// (2)
		reinterpret_cast<void*>(null_task_),	nullptr,	// (3)

		reinterpret_cast<void*>(comp_.itask1),	nullptr,	// (4) コンパレーターB1
		reinterpret_cast<void*>(comp_.itask3),	nullptr,	// (5) コンパレーターB3
		reinterpret_cast<void*>(null_task_),	nullptr,	// (6)
		reinterpret_cast<void*>(null_task_),	nullptr,	// (7) タイマＲＣ

		reinterpret_cast<void*>(null_task_),	nullptr,	// (8)
		reinterpret_cast<void*>(null_task_),	nullptr,	// (9)
		reinterpret_cast<void*>(null_task_),	nullptr,	// (10)
		reinterpret_cast<void*>(null_task_),	nullptr,	// (11)

		reinterpret_cast<void*>(null_task_),	nullptr,	// (12)
		reinterpret_cast<void*>(null_task_),	nullptr,	// (13) キー入力
		reinterpret_cast<void*>(null_task_),	nullptr,	// (14) A/D 変換
		reinterpret_cast<void*>(null_task_),	nullptr,	// (15)

		reinterpret_cast<void*>(null_task_),	nullptr,	// (16)
		reinterpret_cast<void*>(null_task_),	nullptr,	// (17) UART0 送信
		reinterpret_cast<void*>(null_task_),	nullptr,	// (18) UART0 受信
		reinterpret_cast<void*>(null_task_),	nullptr,	// (19)

		reinterpret_cast<void*>(null_task_),	nullptr,	// (20)
		reinterpret_cast<void*>(null_task_),	nullptr,	// (21) /INT2
		reinterpret_cast<void*>(null_task_),	nullptr,	// (22) タイマＲＪ２
		reinterpret_cast<void*>(null_task_),	nullptr,	// (23) 周期タイマ

		reinterpret_cast<void*>(timer_b_.itask),nullptr,	// (24) タイマＲＢ２
		reinterpret_cast<void*>(null_task_),	nullptr,	// (25) /INT1
		reinterpret_cast<void*>(null_task_),	nullptr,	// (26) /INT3
		reinterpret_cast<void*>(null_task_),	nullptr,	// (27)

		reinterpret_cast<void*>(null_task_),	nullptr,	// (28)
		reinterpret_cast<void*>(null_task_),	nullptr,	// (29) /INT0
		reinterpret_cast<void*>(null_task_),	nullptr,	// (30)
		reinterpret_cast<void*>(null_task_),	nullptr,	// (31)
	};
}

int main(int argc, char *argv[])
{
	using namespace device;

// クロック関係レジスタ・プロテクト解除
	PRCR.PRC0 = 1;

// 高速オンチップオシレーターへ切り替え(20MHz)
// ※ F_CLK を設定する事（Makefile内）
	OCOCR.HOCOE = 1;
	utils::delay::micro_second(1);  // >=30us(125KHz)
	SCKCR.HSCKSEL = 1;
	CKSTPR.SCKSEL = 1;

	// 外部に出力
//	EXCKCR.CKPT = 2;

	// タイマー割り込み設定
	{
		uint8_t ir_lvl = 1;
		timer_b_.start_timer(60, ir_lvl);
	}

	// コンパレーター３設定
	{
		comp_.start3();
		utils::PORT_MAP(utils::port_map::P33::IVCMP3);
		utils::PORT_MAP(utils::port_map::P34::IVREF3);
	}

	// メイン
	PD1.B0 = 1;

	uint8_t n = 0;
	uint8_t c = 60;
	while(1) {
		timer_b_.sync();
		if(n < (c / 3)) P1.B0 = 0; 
		else P1.B0 = 1;
		if(comp_.get_value3()) {
			c = 60;
		} else {
			c = 30;
		}
		++n;
		if(n >= c) n = 0;
	}
}