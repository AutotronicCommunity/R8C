//=====================================================================//
/*!	@file
	@brief	R8C SD モニター
	@author	平松邦仁 (hira@rvf-rc45.net)
*/
//=====================================================================//
#include "main.hpp"
#include "system.hpp"
#include "clock.hpp"
#include "common/delay.hpp"
#include "common/port_map.hpp"
#include "common/command.hpp"
#include <cstring>
#include "common/format.hpp"
#include "pfatfs/src/pff.h"

static timer_c timer_c_;

struct wave_t {
	uint8_t	left;
	uint8_t	right;
};

static volatile wave_t wave_buff_[256];
static volatile uint8_t wave_put_ = 0;
static volatile uint8_t wave_get_ = 0;
class wave_out {
	public:
	void operator() () {
		const volatile wave_t& t = wave_buff_[wave_get_];
		timer_c_.set_pwm_b(t.left);
		timer_c_.set_pwm_c(t.right);
		++wave_get_;		
	}
};

typedef device::trb_io<wave_out> timer_audio;
static uart0 uart0_;
static spi_base spi_base_;
static spi_ctrl spi_ctrl_;
static timer_audio timer_b_;

extern "C" {
	void sci_putch(char ch) {
		uart0_.putch(ch);
	}

	char sci_getch(void) {
		return uart0_.getch();
	}

	uint16_t sci_length() {
		return uart0_.length();
	}

	void sci_puts(const char* str) {
		uart0_.puts(str);
	}
}

extern "C" {
	const void* variable_vectors_[] __attribute__ ((section (".vvec"))) = {
		(void*)brk_inst_,   nullptr,	// (0)
		(void*)null_task_,  nullptr,	// (1) flash_ready
		(void*)null_task_,  nullptr,	// (2)
		(void*)null_task_,  nullptr,	// (3)

		(void*)null_task_,  nullptr,	// (4) コンパレーターB1
		(void*)null_task_,  nullptr,	// (5) コンパレーターB3
		(void*)null_task_,  nullptr,	// (6)
		(void*)null_task_,  nullptr,	// (7) タイマＲＣ

		(void*)null_task_,  nullptr,	// (8)
		(void*)null_task_,  nullptr,	// (9)
		(void*)null_task_,  nullptr,	// (10)
		(void*)null_task_,  nullptr,	// (11)

		(void*)null_task_,  nullptr,	// (12)
		(void*)null_task_,  nullptr,	// (13) キー入力
		(void*)null_task_,  nullptr,	// (14) A/D 変換
		(void*)null_task_,  nullptr,	// (15)

		(void*)null_task_,  nullptr,	// (16)
		(void*)uart0_.send_task, nullptr,   // (17) UART0 送信
		(void*)uart0_.recv_task, nullptr,   // (18) UART0 受信
		(void*)null_task_,  nullptr,	// (19)

		(void*)null_task_,  nullptr,	// (20)
		(void*)null_task_,  nullptr,	// (21) /INT2
		(void*)null_task_,  nullptr,	// (22) タイマＲＪ２
		(void*)null_task_,  nullptr,	// (23) 周期タイマ

		(void*)timer_b_.itask,  nullptr,	// (24) タイマＲＢ２
		(void*)null_task_,  nullptr,	// (25) /INT1
		(void*)null_task_,  nullptr,	// (26) /INT3
		(void*)null_task_,  nullptr,	// (27)

		(void*)null_task_,  nullptr,	// (28)
		(void*)null_task_,  nullptr,	// (29) /INT0
		(void*)null_task_,  nullptr,	// (30)
		(void*)null_task_,  nullptr,	// (31)
	};
}

static FATFS fatfs_;

//  __attribute__ ((section (".exttext")))
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

	// ＰＷＭモード設定
	{
		utils::PORT_MAP(utils::port_map::P12::TRCIOB);
		utils::PORT_MAP(utils::port_map::P13::TRCIOC);
		bool pfl = 0;  // 0->1
		timer_c_.start_pwm(0x100, 0, pfl);
//		uint16_t n = timer_c_.get_pwm_limit();
	}

	// タイマーＢ初期化
	{
		uint8_t ir_level = 2;
		timer_b_.start_timer(12000, ir_level);
	}

	// UART の設定 (P1_4: TXD0[out], P1_5: RXD0[in])
	// ※シリアルライターでは、RXD 端子は、P1_6 となっているので注意！
	{
		utils::PORT_MAP(utils::port_map::P14::TXD0);
		utils::PORT_MAP(utils::port_map::P15::RXD0);
		uint8_t intr_level = 1;
		uart0_.start(19200, intr_level);
	}

	// spi_base, spi_ctrl ポートの初期化
	{
		spi_ctrl_.init();
		spi_base_.init();
	}

	sci_puts("Start R8C SD monitor\n");

	bool mount = false;
	// pfatfs を開始
	{
		if(pf_mount(&fatfs_) != FR_OK) {
			sci_puts("SD mount error\n");
		} else {
			sci_puts("SD mount OK!\n");
			mount = true;
		}
	}

	if(mount) {
		DIR dir;
		if(pf_opendir(&dir, "") != FR_OK) {
			sci_puts("Can't open dir\n");
		} else {

			for(;;) {
				FILINFO fno;
				// Read a directory item
				if(pf_readdir(&dir, &fno) != FR_OK) {
					sci_puts("Can't read dir\n");
					break;
				}
				if(!fno.fname[0]) break;

				if(fno.fattrib & AM_DIR) {
					utils::format("          /%s\n") % fno.fname;
				} else {
					utils::format("%8d  %s\n") % static_cast<uint32_t>(fno.fsize) % fno.fname;
				}
			}
		}

		const char* file_name = "OHAYODEL.WAV";
		if(pf_open(file_name) != FR_OK) {
			sci_puts("Can't open file: '");
			sci_puts(file_name);
			sci_puts("'\n");
		} else {
			sci_puts("Play WAVE: '");
			sci_puts(file_name);
			sci_puts("'\n");

			uint16_t n = 0;
			while(1) {
				uint8_t d;
				do {
					timer_b_.sync();
					d = wave_get_ - wave_put_;
				} while(d < 128) ;

				UINT br;
				if(pf_read((void*)&wave_buff_[wave_put_], 128 * 2, &br) == FR_OK) {
					if(br == 0) break;
					wave_put_ += 128;
					++n;
					if(n >= (12000 / 128)) {
						n = 0;
						sci_putch('.');
					}
				} else {
					break;
				}
			}
		}
	}

	sci_puts("End play.\n");
	while(1) ;
}