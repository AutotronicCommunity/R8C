//=====================================================================//
/*!	@file
	@brief	R8C AD9851 サンプル @n
			サインスマート AD9851 モジュール結線情報(R8C/M120AN) @n
			・VCC   <---> VCC(7) 3.3V @n
			・DGND  <---> VSS(5) @n
			・SDATA <---- P1_0(20) @n
			・SCLK  <---- P1_1(19) @n
			・FSYNC <---- P1_2(18) @n
			---------------------- @n
			※８ビット、１ストップビット、パリティ無し、５７６００ボー
			・RXD   <---- P1_4(16):TXD0 @n
			・TXD   ----> P1_5(15):RXD0 @n
			※リセット、モード端子は、ハードウェアーマニュアルを参照の事
    @author 平松邦仁 (hira@rvf-rc45.net)
	@copyright	Copyright (C) 2018 Kunihito Hiramatsu @n
				Released under the MIT license @n
				https://github.com/hirakuni45/R8C/blob/master/LICENSE
*/
//=====================================================================//
#include "system.hpp"
#include "clock.hpp"
#include "common/delay.hpp"
#include "common/intr_utils.hpp"
#include "common/port_map.hpp"
#include "common/fifo.hpp"
#include "common/uart_io.hpp"
#include "common/trb_io.hpp"
#include "common/command.hpp"
#include "common/format.hpp"
#include "common/input.hpp"
#include "common/spi_io.hpp"
#include "chip/AD9851.hpp"

// インジケーターＬＥＤ点滅を行う場合
// #define INDICATOR_LED

namespace {

#ifdef INDICATOR_LED
	typedef device::PORT<device::PORT1, device::bitpos::B3> LED;
#endif

	typedef device::trb_io<utils::null_task, uint8_t> timer_b;
	timer_b timer_b_;

	typedef utils::fifo<uint8_t, 16> buffer;
	typedef device::uart_io<device::UART0, buffer, buffer> uart;
	uart uart_;

	// P1_0(20):
	typedef device::PORT<device::PORT1, device::bitpos::B0> W_CLK;
	// P1_1(19):
	typedef device::PORT<device::PORT1, device::bitpos::B1> FQ_UD;
	// P1_2(18):
	typedef device::PORT<device::PORT1, device::bitpos::B2> D7;
	// P1_3(17):
	typedef device::PORT<device::PORT1, device::bitpos::B3> RESET;

	typedef chip::AD9851<W_CLK, FQ_UD, D7, RESET> AD9851;
	AD9851	ad9851_;

	utils::command<64> command_;
}

extern "C" {

	void sci_putch(char ch) {
		uart_.putch(ch);
	}


	char sci_getch(void) {
		return uart_.getch();
	}


	uint16_t sci_length() {
		return uart_.length();
	}


	void sci_puts(const char* str) {
		uart_.puts(str);
	}


	void TIMER_RB_intr(void) {
		timer_b_.itask();
	}


	void UART0_TX_intr(void) {
		uart_.isend();
	}


	void UART0_RX_intr(void) {
		uart_.irecv();
	}
}


//__attribute__ ((section (".exttext")))
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

	// タイマーＢ初期化
	{
		uint8_t ir_level = 2;
		timer_b_.start_timer(60, ir_level);
	}

	// UART の設定 (P1_4: TXD0[out], P1_5: RXD0[in])
	// ※シリアルライターでは、RXD 端子は、P1_6 となっているので注意！
	{
		utils::PORT_MAP(utils::port_map::P14::TXD0);
		utils::PORT_MAP(utils::port_map::P15::RXD0);
		uint8_t ir_level = 1;
		uart_.start(57600, ir_level);
	}

	{  // AD9851 開始
		ad9851_.start();
	}

	utils::format("Start R8C AD9851 sample\n");
	command_.set_prompt("# ");

#ifdef INDICATOR_LED
	LED::DIR = 1;
	uint8_t cnt = 0;
#endif

	while(1) {
		timer_b_.sync();

#ifdef INDICATOR_LED
		++cnt;
		if(cnt >= 30) {
			cnt = 0;
		}
		if(cnt < 10) LED::P = 1;
		else LED::P = 0;
#endif

		// コマンド入力と、コマンド解析
		if(command_.service()) {
			bool error = false;
			char emsg[16];
			emsg[0] = 0;
			uint8_t cmdn = command_.get_words();
			if(cmdn >= 1) {
				if(command_.cmp_word(0, "freq")) {
					if(cmdn == 1) {
//						utils::format("freq: %1.3f\n") % freq_;
					} else {
						float a = 0.0f;
						command_.get_word(1, sizeof(emsg), emsg);
						if((utils::input("%f", emsg) % a).status()) {
//							freq_ = a;
//							setup_();
						} else {
							error = true;							
						}
					}
				} else if(command_.cmp_word(0, "help")) {
					utils::format("freq [xxxx(Hz)]\n");
				} else {
					command_.get_word(0, sizeof(emsg), emsg);
					error = true;
				}
				if(error) {					
					utils::format("Command error: '%s'\n") % emsg;
				}
			}
		}
	}
}