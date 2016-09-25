//=====================================================================//
/*!	@file
	@brief	R8C BMP180 サンプル
	@author	平松邦仁 (hira@rvf-rc45.net)
*/
//=====================================================================//
#include "system.hpp"
#include "clock.hpp"
#include "common/intr_utils.hpp"
#include "common/port_map.hpp"
#include "common/fifo.hpp"
#include "common/uart_io.hpp"
#include "common/trb_io.hpp"
#include "chip/BMP180.hpp"
#include "common/command.hpp"
#include "common/format.hpp"

namespace {

	typedef device::trb_io<utils::null_task> timer_b;
	timer_b timer_b_;

	typedef utils::fifo<uint8_t, 16> buffer;
	typedef device::uart_io<device::UART0, buffer, buffer> uart;
	uart uart_;

	// I2C ポートの定義クラス
	// P1_B7: SCL
	// P4_B5: SDA
	struct scl_sda {
		void init() const {
			utils::PORT_MAP(utils::port_map::P17::PORT);
			utils::PORT_MAP(utils::port_map::P45::PORT);
			device::POD1.B7 = 1;  // オープン・ドレイン設定
			device::POD4.B5 = 1;  // オープン・ドレイン設定
		}
		void scl_dir(bool b) const { device::PD1.B7 = b; }  // SCL 方向 (0:in, 1:out)
		void scl_out(bool b) const { device::P1.B7 = b; }   // SCL 出力
		bool scl_inp() const { return device::P1.B7(); }    // SCL 入力
		void sda_dir(bool b) const { device::PD4.B5 = b; }  // SDA 方向 (0:in, 1:out)
		void sda_out(bool b) const { device::P4.B5 = b; }   // SDA 出力
		bool sda_inp() const { return device::P4.B5(); }    // SDA 入力
	};

	typedef device::iica_io<scl_sda> iica;
	iica i2c_;
	chip::BMP180<iica> bmp180_(i2c_);

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
}

extern "C" {
	const void* variable_vectors_[] __attribute__ ((section (".vvec"))) = {
		reinterpret_cast<void*>(brk_inst_),		nullptr,	// (0)
		reinterpret_cast<void*>(null_task_),	nullptr,	// (1) flash_ready
		reinterpret_cast<void*>(null_task_),	nullptr,	// (2)
		reinterpret_cast<void*>(null_task_),	nullptr,	// (3)

		reinterpret_cast<void*>(null_task_),	nullptr,	// (4) コンパレーターB1
		reinterpret_cast<void*>(null_task_),	nullptr,	// (5) コンパレーターB3
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
		reinterpret_cast<void*>(uart_.isend),	nullptr,	// (17) UART0 送信
		reinterpret_cast<void*>(uart_.irecv),	nullptr,	// (18) UART0 受信
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

	// I2C クラスの初期化
	{
		i2c_.start(iica::speed::fast);
	}

	// BMP180 を開始
	{
		if(!bmp180_.start()) {
			utils::format("Stall BMP180 start (%d)\n") % static_cast<uint32_t>(i2c_.get_last_error());
		}
	}

	sci_puts("Start R8C BMP180 sample\n");
	command_.set_prompt("# ");

	// LED シグナル用ポートを出力
	PD1.B0 = 1;

	uint8_t n = 0;
	uint8_t cnt = 0;
	while(1) {
		timer_b_.sync();

		if(cnt >= 20) {
			cnt = 0;
		}
		if(cnt < 10) P1.B0 = 1;
		else P1.B0 = 0;
		++cnt;

		++n;
		if(n >= 60) {
			n = 0;

			auto t = bmp180_.get_temperature();
			utils::format("Temperature: %d.%d C\n") % (t / 10) % (t % 10);

			auto p = bmp180_.get_pressure();
			utils::format("Pressure: %d.%02d hPa\n") % (p / 100) % (p % 100);
		}

		// コマンド入力と、コマンド解析
		if(command_.service()) {
			uint8_t cmdn = command_.get_words();
			if(cmdn >= 1) {
//				if(command_.cmp_word(0, "date")) {
//				} else if(command_.cmp_word(0, "help")) {
//					sci_puts("date\n");
//					sci_puts("date yyyy/mm/dd hh:mm[:ss]\n");
//				} else {
//					char buff[12];
//					if(command_.get_word(0, sizeof(buff), buff)) {
//						sci_puts("Command error: ");
//						sci_puts(buff);
//						sci_putch('\n');
//					}
//				}
			}
		}
	}
}